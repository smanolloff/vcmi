#include <boost/chrono/time_point.hpp>
#include <chrono>
#include <random>
#include <cassert>
#include <sqlite3.h>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "Stats.h"
#include "constants/Enumerations.h"

#define _MEASURE_START(IDENT) \
    auto measurement_start_##IDENT = std::chrono::high_resolution_clock::now(); \
    logStats->trace("%s: started", measurement_name_##IDENT);

#define MEASURE_START(IDENT) \
    std::string measurement_name_##IDENT = #IDENT; \
    _MEASURE_START(IDENT)

#define MEASURE_START2(IDENT, NAME) \
    auto measurement_name_##IDENT = std::string(#IDENT ": ") + NAME; \
    _MEASURE_START(IDENT)

#define MEASURE_END(IDENT, WARN_MS) { \
    auto measurement_end_##IDENT = std::chrono::high_resolution_clock::now(); \
    std::chrono::duration<float, std::milli> measurement_duration_##IDENT = measurement_end_##IDENT - measurement_start_##IDENT; \
    auto measurement_loglvl_##IDENT = measurement_duration_##IDENT.count() < WARN_MS ? ELogLevel::TRACE : ELogLevel::WARN; \
    logStats->log(measurement_loglvl_##IDENT, "%s: took %.0f ms", measurement_name_##IDENT, measurement_duration_##IDENT.count()); \
}


VCMI_LIB_NAMESPACE_BEGIN

namespace ML {
    void Error(const char* format, ...) {
        constexpr int bufferSize = 2048; // Adjust this size according to your needs
        char buffer[bufferSize];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, bufferSize, format, args);
        va_end(args);

        logStats->error(buffer);
        throw std::runtime_error(buffer);
    }

    // function for executing SQL string literals
    std::pair<int, std::string> TryExecSQL(sqlite3* db, const char* sql) {
        MEASURE_START2(ExecSQL, sql);
        char* err = nullptr;
        auto rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        MEASURE_END(ExecSQL, 1000);
        auto errstr = err ? std::string(err) : "";
        sqlite3_free(err);
        return {rc, errstr};
    }

    // function for executing SQL string literals
    void ExecSQL(sqlite3* db, const char* sql) {
        auto [rc, err] = TryExecSQL(db, sql);
        if (rc != SQLITE_OK) {
            std::string error = "dbexec: sqlite3_exec: " + err;
            logStats->error(error);
            throw std::runtime_error(error);
        }
    }

    void Stats::withinTransaction(sqlite3* db, bool commit, std::function<void()> func) {
        auto sqlstr = std::string("PRAGMA busy_timeout = ") + std::to_string(timeout);
        ExecSQL(db, sqlstr.c_str());

        while (true) {
            auto [rc, err] = TryExecSQL(db, "BEGIN IMMEDIATE TRANSACTION");
            if (rc == SQLITE_BUSY) {
                logStats->warn("Failed to obtain DB lock within %dms, retrying in 5s...", timeout);
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            func();

            if (commit)
                ExecSQL(db, "COMMIT");

            break;
        }
    }

    Stats::Stats(
        std::string dbpath_,
        int side,
        int timeout_,
        int persistfreq_,
        int maxbattles_
    ) : dbpath(dbpath_)
      , timeout(timeout_)
      , persistfreq(persistfreq_)
      , maxbattles(maxbattles_)
      , persistcounter(persistfreq_)
    {
        buffer.reserve(maxbattles);

        // TODO: make parameter to optionally skip
        // verify(side);
    }

    void Stats::verify(int side) {
        // test DB structure and verify side against the DB metadata

        sqlite3* db;
        if (sqlite3_open(dbpath.c_str(), &db))
            Error("sqlite3_open: %s", sqlite3_errmsg(db));

        withinTransaction(db, false, [db, side] {
            ExecSQL(db, "INSERT INTO stats (lhero, rhero, wins, games) VALUES (0,0,0,0)");

            sqlite3_stmt* stmt;
            const char* sql = "SELECT side FROM stats_md";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
                Error("sqlite3_prepare_v2: %s", sqlite3_errmsg(db));

            auto rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
                Error("init: side check failed: no rows found in stats_md table");
            else if (rc != SQLITE_ROW)
                Error("init: side check failed: bad status: want: %d, have: %d, errmsg: %s", SQLITE_ROW, rc, sqlite3_errmsg(db));

            auto dbside = sqlite3_column_int(stmt, 0);

            if (dbside != side)
                Error("init: side check failed: side in DB is %d, want: %d", dbside, side);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
                Error("init: side check failed: extra rows found in stats_md table", SQLITE_DONE, rc, sqlite3_errmsg(db));

            rc = sqlite3_reset(stmt);
            if (rc != SQLITE_OK)
                Error("init: sqlite3_reset: bad status: want: %d, have: %d, errmsg: %s", SQLITE_OK, rc, sqlite3_errmsg(db));

            rc = sqlite3_finalize(stmt);
            if (rc != SQLITE_OK)
                Error("init: sqlite3_finalize: bad status: want: %d, have: %d, errmsg: %s", SQLITE_OK, rc, sqlite3_errmsg(db));

            ExecSQL(db, "ROLLBACK");
        });

        if (sqlite3_close(db))
            Error("sqlite3_close: %s", sqlite3_errmsg(db));
    }

    void Stats::dbupdate() {
        MEASURE_START(dbupdate);
        logStats->info("Updating database at %s", dbpath.c_str());

        sqlite3* db;
        if (sqlite3_open(dbpath.c_str(), &db))
            Error("dbupdate: sqlite3_open: %s", sqlite3_errmsg(db));

        try {
            withinTransaction(db, true, [this, db] {
                sqlite3_stmt* stmt;

                // XXX:
                // INSERT .. ON CONFLICT (...) DO UPDATE ...
                // is slower than a plain UPDATE
                // => all rows must preemptively be inserted into the DB
                //    (use sql/seed.sql)

                // const char* sql =
                //     "INSERT INTO stats (lhero, rhero, wins, games) "
                //     "VALUES (?, ?, ?, ?, ?) "
                //     "ON CONFLICT(lhero, rhero) "
                //     "DO UPDATE SET wins = excluded.wins + wins, games = excluded.games + games";

                const char* sql =
                    "UPDATE stats "
                    "SET wins=wins+?, games=games+? "
                    "WHERE lhero=? AND rhero=? "
                    "RETURNING id";

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
                    Error("sqlite3_prepare_v2: %s", sqlite3_errmsg(db));

                logStats->trace("Prepared: %s", sql);

                for (auto &[k, v] : buffer) {
                    auto [lhero, rhero] = k;
                    auto [wins, games] = v;
                    sqlite3_bind_int(stmt, 1, wins);
                    sqlite3_bind_int(stmt, 2, games);
                    sqlite3_bind_int(stmt, 3, lhero);
                    sqlite3_bind_int(stmt, 4, rhero);

                    logStats->trace("Bindings: %d %d %d %d", wins, games, lhero, rhero);

                    auto rc = sqlite3_step(stmt);
                    if (rc != SQLITE_ROW)
                        Error("sqlite3_step: bad status: want: %d, have: %d, errmsg: %s", SQLITE_ROW, rc, sqlite3_errmsg(db));

                    // auto dbid = sqlite3_column_int(stmt, 0);

                    // there should be no more rows returned
                    rc = sqlite3_step(stmt);
                    if (rc != SQLITE_DONE)
                        Error("sqlite3_step: bad status: want: %d, have: %d, errmsg: %s", SQLITE_DONE, rc, sqlite3_errmsg(db));

                    rc = sqlite3_reset(stmt);
                    if (rc != SQLITE_OK)
                        Error("sqlite3_reset: bad status: want: %d, have: %d, errmsg: %s", SQLITE_OK, rc, sqlite3_errmsg(db));
                }

                auto rc = sqlite3_finalize(stmt);
                if (rc != SQLITE_OK)
                    Error("sqlite3_finalize: bad status: want: %d, have: %d, errmsg: %s", SQLITE_OK, rc, sqlite3_errmsg(db));
                buffer.clear();
            });
        } catch (const std::exception& e) {
            std::cerr << "dbupdate: " << e.what() << std::endl;
            auto rc = sqlite3_close(db);
            if (rc != SQLITE_OK)
                logStats->error("sqlite3_close: bad status: want: %d, have: %d, errmsg: %s", SQLITE_OK, rc, sqlite3_errmsg(db));
            throw;
        }

        auto rc = sqlite3_close(db);
        if (rc != SQLITE_OK)
            Error("sqlite3_close: bad status: want: %d, have: %d, errmsg: %s", SQLITE_OK, rc, sqlite3_errmsg(db));

        MEASURE_END(dbupdate, 5000);
    }

    void Stats::dataadd(bool victory, int heroL, int heroR) {
        logStats->debug("Adding data: victory=%d heroL=%d heroR=%d", victory, heroL, heroR);
        auto key = std::tuple<int, int> {heroL, heroR};
        auto it = buffer.find(key);
        if(it == buffer.end()) {
            buffer.insert({key, {static_cast<int>(victory), 1}});
        } else {
            auto &value = it->second;
            std::get<0>(value) += static_cast<int>(victory);
            std::get<1>(value) += 1;
        }

        logStats->trace("persistcounter: %d", persistcounter);
        if (--persistcounter == 0) {
            persistcounter = persistfreq;
            dbupdate();
        }
    }
}

VCMI_LIB_NAMESPACE_END
