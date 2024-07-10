#pragma once

#include <tuple>
#include <unordered_map>
#include <vector>
#include <map>
#include <sqlite3.h>
#include "StdInc.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {
    // A custom hash function must be provided for the buffer
    struct TupleHash {
        std::size_t operator()(const std::tuple<int, int, int>& t) const {
            auto hash1 = std::hash<int>{}(std::get<0>(t));
            auto hash2 = std::hash<int>{}(std::get<1>(t));
            auto hash3 = std::hash<int>{}(std::get<2>(t));
            return hash1 ^ (hash2 << 1) ^ (hash3 << 2);
        }
    };

    class DLL_LINKAGE Stats {
    public:
        using N_WINS = int;
        using N_GAMES = int;
        using STAT = std::tuple<N_WINS, N_GAMES>;

        // XXX: dbpath must be unuque for agent, map and perspective
        Stats(std::string dbpath, int persistfreq, int maxbattles);

        void dbupdate(); // add buffers data to DB
        void dataadd(bool side, bool victory, int heroL, int heroR);
    private:
        std::unordered_map<std::tuple<int, int, int>, std::tuple<int, int>, TupleHash> buffer;
        const std::string dbpath;
        const int maxbattles;
        const int persistfreq;
        int persistcounter;
    };
}

VCMI_LIB_NAMESPACE_END
