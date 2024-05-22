#pragma once

#include <tuple>
#include <vector>
#include <map>
#include <sqlite3.h>

VCMI_LIB_NAMESPACE_BEGIN


class Stats {
public:
    using N_WINS = int;
    using N_GAMES = int;
    using STAT = std::tuple<N_WINS, N_GAMES>;

    // XXX: dbpath must be unuque for agent, map and perspective
    Stats(int nheroes, std::string dbpath, int persistfreq, int redistfreq, float scorevar);
    ~Stats();

    void dataadd(bool side, bool victory, int heroL, int heroR);
    void dump(bool side, bool nonzero = false);
    int sample1(bool side);
    std::pair<int, int> sample2(bool side);

private:
    // id=heroL, value={wins, games}
    std::vector<STAT> data1L;

    // id=heroR, value={wins, games}
    std::vector<STAT> data1R;

    // id=heroL, value=ratio_heroL_vs_all
    std::vector<float> scores1L;

    // id=heroR, value=ratio_heroR_vs_all
    std::vector<float> scores1R;

    // id=local_id, value=ratio_heroL_vs_heroR
    std::vector<float> scores2L;

    // id=local_id, value=ratio_heroR_vs_heroL
    std::vector<float> scores2R;

    // db_ids[X] => the db ID for permutation X
    // however, there is no guarantee that db_ids correspond to permutation#
    // => use db_ids mapping (index=permutation#, value=db_id)
    // using a plain vector as permutation# values are incremental
    std::vector<int> local_to_db_refs_L;
    std::vector<int> local_to_db_refs_R;

    // Inverse mapping of the above (key=db_id, value=permutation#)
    // can't use a vector: db values are not incremental
    std::map<int, int> db_to_local_refs_L;
    std::map<int, int> db_to_local_refs_R;

    std::random_device rd {};
    std::mt19937 gen;
    std::discrete_distribution<> dist1L;
    std::discrete_distribution<> dist1R;
    std::discrete_distribution<> dist2L;
    std::discrete_distribution<> dist2R;
    std::string dbpath;
    int nheroes;

    // Persist entire DB to disk once every N `dataadd` calls (i.e. N battles)
    // On mac this takes ~2s for 4096-hero map (33M records). On pc - untested
    // On mac there are 1~3 battles/sec
    // => 300 battles = 100~300sec => 2s save will be a <2% slowdown
    int persistfreq;
    int persistcounter;

    // Redistribute scores into discrete distributions
    // On mac this takes ~1s for 4096-hero map. On pc - untested
    int redistfreq;
    std::array<int, 2> redistcounters;

    float scorevar;
    float minscore;
    float maxscore;

    sqlite3* memdb;

    static inline void Error(const char* format, ...);

    float calcscore(int wins, int games);
    void rescore();
    void redistribute(bool side);

    void dbinit(int heroes);        // init memdb
    void dbrestore();               // filedb -> memdb
    void dbpersist();               // memdb -> filedb
    void dataload(int nheroes);     // memdb -> vars
    void dbexec(const char* sql);
    void with_filedb(std::function<void(sqlite3*)> callback);
    void with_stmt(const char *sql, std::function<void(sqlite3_stmt*)> callback);
};

VCMI_LIB_NAMESPACE_END
