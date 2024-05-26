#include <random>
#include <cassert>
#include <sqlite3.h>
#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "stats.h"

VCMI_LIB_NAMESPACE_BEGIN

void Stats::Error(const char* format, ...) {
    constexpr int bufferSize = 2048; // Adjust this size according to your needs
    char buffer[bufferSize];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, bufferSize, format, args);
    va_end(args);

    throw std::runtime_error(buffer);
}

Stats::Stats(int nheroes_, std::string dbpath_, int persistfreq_, int redistfreq_, float scorevar_)
: gen(rd()),
  nheroes(nheroes_),
  dbpath(dbpath_),
  persistfreq(persistfreq_),
  redistfreq(redistfreq_),
  scorevar(scorevar_)
{
    assert(nheroes > 1);

    // comparing with 0.1 and 0.4 wont work for floats
    assert(scorevar > 0 && scorevar < 0.5);

    persistcounter = persistfreq;
    redistcounters = {redistfreq, redistfreq};
    minscore = 0.5 - scorevar;
    maxscore = 0.5 + scorevar;

    // redistfreq is *2 as there are separate redistcounters for each side
    updatebuffers.at(0).reserve(std::max<int>(persistfreq, redistfreq));
    updatebuffers.at(1).reserve(std::max<int>(persistfreq, redistfreq));

    // We are using permutation instead of combination as the data
    // for heroL&heroR is different than the data for heroR&heroL
    // (heroes are the same, but have different controllers)
    auto npermutations = nheroes * (nheroes - 1);

    data1L.insert(data1L.begin(), nheroes, {0, 0});
    data1R.insert(data1R.begin(), nheroes, {0, 0});
    scores1L.insert(scores1L.begin(), nheroes, 1);
    scores1R.insert(scores1R.begin(), nheroes, 1);

    local_to_db_refs_L.reserve(npermutations);
    local_to_db_refs_R.reserve(npermutations);
    scores2L.reserve(npermutations);
    scores2R.reserve(npermutations);

    if (sqlite3_open(":memory:", &memdb))
        Error("memdb sqlite3_open error: %s", sqlite3_errmsg(memdb));

    if (std::filesystem::is_regular_file(std::filesystem::path(dbpath))) {
        dbrestore();
    } else {
        dbinit(nheroes);
        dbpersist();
    }

    dataload(nheroes);

    assert(local_to_db_refs_L.size() == npermutations);
    assert(local_to_db_refs_R.size() == npermutations);
    assert(scores2L.size() == npermutations);
    assert(scores2R.size() == npermutations);

    redistribute(0);
    redistribute(1);
}

Stats::~Stats() {
    if (memdb) {
        sqlite3_close(memdb);
        logStats->debug("DB connection closed");
    }
}

// function for executing SQL string literals
void Stats::dbexec(const char* sql) {
    char* err = nullptr;
    logStats->trace("Executing query: %s", sql);
    if (sqlite3_exec(memdb, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string error = "dbexec sqlite3_exec error: " + std::string(err);
        sqlite3_free(err);
        throw std::runtime_error(error);
    }
}

void Stats::with_stmt(const char *sql, std::function<void(sqlite3_stmt*)> callback) {
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(memdb, sql, -1, &stmt, nullptr) != SQLITE_OK)
        Error("with_stmt sqlite3_prepare_v2 error: %s", sqlite3_errmsg(memdb));

    logStats->trace("Prepared statement: %s", sql);

    try {
        callback(stmt);
    } catch (const std::exception& e) {
        std::cerr << "with_stmt callback error: " << e.what() << std::endl;
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
}

void Stats::with_filedb(std::function<void(sqlite3*)> callback) {
    sqlite3* filedb;
    if (sqlite3_open(dbpath.c_str(), &filedb))
        Error("filedb sqlite3_open error: %s", sqlite3_errmsg(filedb));

    try {
        callback(filedb);
    } catch (const std::exception& e) {
        std::cerr << "with_filedb callback error: " << e.what() << std::endl;
        sqlite3_close(filedb);
        throw;
    }

    sqlite3_close(filedb);
}

void Stats::dbrestore() {
    logStats->info("Restoring in-memory database from %s", dbpath);

    with_filedb([this](sqlite3* filedb) {
        auto backup = sqlite3_backup_init(memdb, "main", filedb, "main");
        if (!backup)
            Error("dbrestore sqlite3_backup_init error: %s", sqlite3_errmsg(filedb));

        if (sqlite3_backup_step(backup, -1) != SQLITE_DONE)
            Error("dbrestore sqlite3_backup_step error: %s", sqlite3_errmsg(filedb));

        if (sqlite3_backup_finish(backup) != SQLITE_OK)
            Error("dbrestore sqlite3_backup_finish error: %s", sqlite3_errmsg(filedb));
    });
}

void Stats::dbpersist() {
    // Flush vectors to in-memory db and update scores2
    flushbuffers(0);
    flushbuffers(1);

    if (dbpath == "-")
        return;  // in-memory only

    logStats->info("Persisting in-memory database to %s", dbpath);

    with_filedb([this](sqlite3* filedb) {
        auto backup = sqlite3_backup_init(filedb, "main", memdb, "main");
        if (!backup)
            Error("dbpersist sqlite3_backup_init error: %s", sqlite3_errmsg(filedb));

        if (sqlite3_backup_step(backup, -1) != SQLITE_DONE)
            Error("dbpersist sqlite3_backup_step error: %s", sqlite3_errmsg(filedb));

        if (sqlite3_backup_finish(backup) != SQLITE_OK)
            Error("dbpersist sqlite3_backup_finish error: %s", sqlite3_errmsg(filedb));
    });
}

void Stats::dbinit(int nheroes) {
    logStats->info("Initializing DB");

    dbexec("BEGIN");
    dbexec(
        "CREATE TABLE stats ("
        "id INTEGER primary key"
        ", side INTEGER NOT NULL"
        ", lhero INTEGER NOT NULL"
        ", rhero INTEGER NOT NULL"
        ", wins INTEGER NOT NULL"
        ", games INTEGER NOT NULL)"
    );

    const char* sql = "INSERT INTO stats (side, lhero, rhero, wins, games) VALUES (?, ?, ?, 0, 0)";

    with_stmt(sql, [this, nheroes](sqlite3_stmt* stmt) {
        for (auto side : {0, 1}) {
            for (int h1=0; h1<nheroes; h1++) {
                for (int h2=0; h2<nheroes; h2++) {
                    if (h1 == h2) continue;
                    sqlite3_bind_int(stmt, 1, side);
                    sqlite3_bind_int(stmt, 2, h1);
                    sqlite3_bind_int(stmt, 3, h2);

                    if (sqlite3_step(stmt) != SQLITE_DONE)
                        Error("Failed to insert data: %s", sqlite3_errmsg(memdb));

                    sqlite3_reset(stmt);
                }
            }
        }
    });

    dbexec("CREATE UNIQUE INDEX stats_idx ON stats(side, lhero, rhero)");
    dbexec("COMMIT");
}

void Stats::dataload(int nheroes) {
    logStats->info("Loading data from DB");
    const char* sql = "SELECT id, side, lhero, rhero, wins, games FROM stats ORDER BY side, lhero, rhero";
    with_stmt(sql, [this, nheroes](sqlite3_stmt* stmt) {
        while (true) {
            auto rc = sqlite3_step(stmt);

            if (rc == SQLITE_DONE)
                break;

            if (rc != SQLITE_ROW)
                Error("dataload error: sqlite3_step status %d != %d, errmsg: %s", rc, SQLITE_ROW, sqlite3_errmsg(memdb));

            int id = sqlite3_column_int(stmt, 0);  // id for the 2-hero combination
            int side = sqlite3_column_int(stmt, 1);
            int heroL = sqlite3_column_int(stmt, 2);
            int heroR = sqlite3_column_int(stmt, 3);
            int wins = sqlite3_column_int(stmt, 4);
            int games = sqlite3_column_int(stmt, 5);

            // printf("Loaded: id=%d, side=%d, heroL=%d, heroR=%d, wins=%d, games=%d\n", id, side, heroL, heroR, wins, games);

            auto &hero = side ? heroR : heroL;
            auto &data1 = side ? data1R : data1L;
            auto &scores1 = side ? scores1R : scores1L;
            auto &scores2 = side ? scores2R : scores2L;
            auto &local_to_db_refs = side ? local_to_db_refs_R : local_to_db_refs_L;
            auto &db_to_local_refs = side ? db_to_local_refs_R : db_to_local_refs_L;

            // ensure we are not loading a wrong DB
            assert(hero < nheroes);
            assert(wins <= games);

            auto &[wins1, games1] = data1.at(hero);
            wins1 += wins;
            games1 += games;
            scores1.at(hero) = calcscore(wins, games);

            scores2.push_back(calcscore(wins, games));
            local_to_db_refs.push_back(id);

            // printf("Referenced side=%d local_id=%zu => db_id=%d\n", side, local_to_db_refs.size(), id);
            db_to_local_refs[id] = local_to_db_refs.size() - 1;
        }
    });
    logStats->debug("Loaded %d rows of data", local_to_db_refs_L.size() + local_to_db_refs_R.size());
}

float Stats::calcscore(int wins, int games) {
    // 100% wins = 0 rawscore, 10% wins = 0.9, etc.
    auto rawscore = (games == 0) ? 1 : (1 - float(wins) / games);
    // XXX: can't change score based on all games played by all heroes
    //      (this means it would require recalculation of all scores on
    //       each game)
    return std::clamp<float>(rawscore, minscore, maxscore);
}

void Stats::dataadd(bool side, bool victory, int heroL, int heroR) {
    logStats->debug("Adding data: side=%d victory=%d heroL=%d heroR=%d", side, victory, heroL, heroR);
    assert(heroL < nheroes && heroR < nheroes);
    auto windiff = static_cast<int>(victory);

    updatebuffers.at(side).emplace_back(windiff, heroL, heroR);

    auto &hero = side ? heroR : heroL;
    auto &data1 = side ? data1R : data1L;
    auto &scores1 = side ? scores1R : scores1L;
    auto &[wins1, games1] = data1.at(hero);

    wins1 += windiff;
    games1 += 1;
    scores1.at(hero) = calcscore(wins1, games1);

    auto &redistcounter = redistcounters.at(side);
    logStats->trace("redistcounter (side=%d): %d", side, redistcounter);
    if (--redistcounter == 0) {
        redistcounter = redistfreq;
        redistribute(side);
    }

    logStats->trace("persistcounter: %d", persistcounter);
    if (--persistcounter == 0) {
        persistcounter = persistfreq;
        dbpersist();
    }
}

void Stats::flushbuffers(int side) {
    auto &buffer = updatebuffers.at(side);
    if (buffer.size() == 0) return;

    logStats->info("Flushing %d updates in data buffer for side %s", buffer.size(), side ? "blue" : "red");

    auto &scores2 = side ? scores2R : scores2L;
    auto &db_to_local_refs = side ? db_to_local_refs_R : db_to_local_refs_L;
    auto sql =
        "UPDATE stats"
        " SET wins = wins+?, games = games+1"
        " WHERE side = ? AND lhero = ? AND rhero = ?"
        " RETURNING id, wins, games";

    dbexec("BEGIN");
    with_stmt(sql, [this, &buffer, &side, &scores2, &db_to_local_refs](sqlite3_stmt* stmt) {
        for (auto &[windiff, heroL, heroR] : buffer) {
            sqlite3_bind_int(stmt, 1, windiff);
            sqlite3_bind_int(stmt, 2, side);
            sqlite3_bind_int(stmt, 3, heroL);
            sqlite3_bind_int(stmt, 4, heroR);

            auto rc = sqlite3_step(stmt);

            if (rc != SQLITE_ROW)
                Error("dataadd error: sqlite3_step status %d != %d, errmsg: %s", rc, SQLITE_ROW, sqlite3_errmsg(memdb));

            auto dbid = sqlite3_column_int(stmt, 0);
            auto wins = sqlite3_column_int(stmt, 1);
            auto games = sqlite3_column_int(stmt, 2);
            auto localid = db_to_local_refs.at(dbid);
            scores2.at(localid) = calcscore(wins, games);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
                Error("dataadd error: sqlite3_step status %d != %d, errmsg: %s", rc, SQLITE_DONE, sqlite3_errmsg(memdb));

            rc = sqlite3_reset(stmt);
            if (rc != SQLITE_OK)
                Error("dataadd error: sqlite3_reset status %d != %d, errmsg: %s", rc, SQLITE_OK, sqlite3_errmsg(memdb));
        }
    });
    dbexec("COMMIT");
    buffer.clear();
}

void Stats::redistribute(bool side) {
    logStats->info("Redistributing scores for side %s", side ? "blue" : "red");
    flushbuffers(side);

    if (side) {
        dist1R = std::discrete_distribution<>(scores1R.begin(), scores1R.end());
        dist2R = std::discrete_distribution<>(scores2R.begin(), scores2R.end());
    } else {
        dist1L = std::discrete_distribution<>(scores1L.begin(), scores1L.end());
        dist2L = std::discrete_distribution<>(scores2L.begin(), scores2L.end());
    }
}

int Stats::sample1(bool side) {
    return side ? dist1R(gen) : dist1L(gen);
}

std::pair<int, int> Stats::sample2(bool side) {
    auto dbid = side
        ? local_to_db_refs_R.at(dist2R(gen))
        : local_to_db_refs_L.at(dist2L(gen));

    const char* sql = "SELECT side, lhero, rhero FROM stats WHERE id = ?";
    int heroL;
    int heroR;

    with_stmt(sql, [this, &dbid, &side, &heroL, &heroR](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, dbid);

        auto rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW)
            Error("sample2 error: sqlite3_step status %d != %d, errmsg: %s", rc, SQLITE_ROW, sqlite3_errmsg(memdb));

        auto dbside = sqlite3_column_int(stmt, 0);
        assert(dbside == side);

        heroL = sqlite3_column_int(stmt, 1);
        heroR = sqlite3_column_int(stmt, 2);


        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
            Error("sample2 error: sqlite3_step status %d != %d, errmsg: %s", rc, SQLITE_DONE, sqlite3_errmsg(memdb));
    });

    return {heroL, heroR};
}

void Stats::dump(bool side, bool nonzero) {
    assert(data1L.size() == data1R.size());

    printf("******* BEGIN DUMP (side=%d) *******\n", side);

    for (int i=0; i<data1L.size(); i++) {
        auto &data1 = side ? data1R : data1L;
        auto &dist1 = side ? dist1R : dist1L;
        auto [wins, games] = data1.at(i);
        auto prob = dist1.probabilities().at(i);
        if (nonzero || games > 0)
            printf("Stats (%d): side=%d, wins=%d, games=%d, prob=%.4f (%.2f%%)\n", i, side, wins, games, prob, prob*100);
    }

    auto sql = std::ostringstream();
    sql << "SELECT id, side, lhero, rhero, wins, games FROM stats WHERE side = " << int(side) << " ORDER BY side, lhero, rhero";

    with_stmt(sql.str().c_str(), [this, &nonzero](sqlite3_stmt* stmt) {
        while (true) {
            auto rc = sqlite3_step(stmt);

            if (rc == SQLITE_DONE)
                break;

            if (rc != SQLITE_ROW)
                Error("dataload sqlite3_step error: status %d != %d, errmsg: %s", rc, SQLITE_ROW, sqlite3_errmsg(memdb));

            auto dbid = sqlite3_column_int(stmt, 0);  // id for the 2-hero combination
            auto side = sqlite3_column_int(stmt, 1);
            auto heroL = sqlite3_column_int(stmt, 2);
            auto heroR = sqlite3_column_int(stmt, 3);
            auto wins = sqlite3_column_int(stmt, 4);
            auto games = sqlite3_column_int(stmt, 5);

            auto &scores2 = side ? scores2R : scores2L;
            auto &dist2 = side ? dist2R : dist2L;
            auto &db_to_local_refs = side ? db_to_local_refs_R : db_to_local_refs_L;

            auto localid = db_to_local_refs.at(dbid);
            auto score = scores2.at(localid);
            auto prob = dist2.probabilities().at(localid);

            // printf("i: %d\n", i);
            // for (auto &[k, v] : db_to_local_ids)
            //     printf("db_id: %d, local_id: %d\n", k, v);

            // for (int x=0; x<db_ids.size(); x++)
            //     printf("local_id=%d, db_id: %d\n", x, db_ids.at(x));

            // for (int x=0; x<dist2.probabilities().size(); x++)
            //     printf("local_id=%d, dist2.probabilities()[x]: %f\n", x, dist2.probabilities().at(x));

            if (nonzero || games > 0)
                printf("Stats (%d, %d): side=%d, wins=%d, games=%d, score=%.2f, prob=%.4f (%.2f%%)\n", heroL, heroR, side, wins, games, score, prob, prob*100);
        }
    });

    printf("******* END DUMP *******\n");
}

VCMI_LIB_NAMESPACE_END
