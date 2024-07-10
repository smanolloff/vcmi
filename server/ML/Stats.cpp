#include <boost/chrono/time_point.hpp>
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
    void ExecSQL(sqlite3* db, const char* sql) {
        MEASURE_START2(ExecSQL, sql);
        char* err = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string error = "dbexec: sqlite3_exec: " + std::string(err);
            sqlite3_free(err);
            logStats->error(error);
            throw std::runtime_error(error);
        }
        MEASURE_END(ExecSQL, 1000);
    }

    Stats::Stats(
        std::string dbpath_,
        int persistfreq_,
        int maxbattles_
    ) : dbpath(dbpath_)
      , persistfreq(persistfreq_)
      , maxbattles(maxbattles_)
      , persistcounter(persistfreq_)
    {
        buffer.reserve(maxbattles);

        // test DB
        sqlite3* db;
        if (sqlite3_open(dbpath.c_str(), &db))
            Error("sqlite3_open: %s", sqlite3_errmsg(db));
        ExecSQL(db, "BEGIN");
        ExecSQL(db, "INSERT INTO stats (side, lhero, rhero, wins, games) VALUES (0,0,0,0,0,0)");
        ExecSQL(db, "ROLLBACK");
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
            ExecSQL(db, "PRAGMA busy_timeout = 60000");
            ExecSQL(db, "BEGIN");
            sqlite3_stmt* stmt;

            // XXX:
            // INSERT .. ON CONFLICT (...) DO UPDATE ...
            // is slower than a plain UPDATE
            // => all rows must preemptively be inserted into the DB
            //    (use sql/seed.sql)

            // const char* sql =
            //     "INSERT INTO stats (side, lhero, rhero, wins, games) "
            //     "VALUES (?, ?, ?, ?, ?) "
            //     "ON CONFLICT(side, lhero, rhero) "
            //     "DO UPDATE SET wins = excluded.wins + wins, games = excluded.games + games";

            const char* sql =
                "UPDATE stats "
                "SET wins=wins+?, games=games+? "
                "WHERE side=? AND lhero=? AND rhero=? "
                "RETURNING id";

            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
                Error("sqlite3_prepare_v2: %s", sqlite3_errmsg(db));

            logStats->trace("Prepared: %s", sql);

            for (auto &[k, v] : buffer) {
                auto [side, lhero, rhero] = k;
                auto [wins, games] = v;
                sqlite3_bind_int(stmt, 1, wins);
                sqlite3_bind_int(stmt, 2, games);
                sqlite3_bind_int(stmt, 3, side);
                sqlite3_bind_int(stmt, 4, lhero);
                sqlite3_bind_int(stmt, 5, rhero);

                logStats->trace("Bindings: %d %d %d %d %d", wins, games, side, lhero, rhero);

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

            ExecSQL(db, "COMMIT");
            buffer.clear();
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

    void Stats::dataadd(bool side, bool victory, int heroL, int heroR) {
        logStats->debug("Adding data: side=%d victory=%d heroL=%d heroR=%d", side, victory, heroL, heroR);
        auto key = std::tuple<int, int, int> {side, heroL, heroR};
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
