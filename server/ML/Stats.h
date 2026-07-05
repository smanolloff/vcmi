// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

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
            auto h0 = std::hash<int>{}(std::get<0>(t));
            auto h1 = std::hash<int>{}(std::get<1>(t));
            auto h2 = std::hash<int>{}(std::get<2>(t));
            return h0 ^ (h1 << 1) ^ (h2 << 2);
        }
    };

    class DLL_LINKAGE Stats {
    public:
        using N_WINS = int;
        using N_GAMES = int;
        using STAT = std::tuple<N_WINS, N_GAMES>;

        // XXX: dbpath must be unuque for agent, map and perspective
        Stats(std::string dbpath, int side, int timeout, int persistfreq, int maxbattles, int npools, int poolsize);

        void dbupdate(); // add buffers data to DB
        void dataadd(bool victory, int pool, int heroL, int heroR);
    private:
        std::unordered_map<std::tuple<int, int, int>, std::tuple<int, int>, TupleHash> buffer;
        void withinTransaction(sqlite3* db, bool commit, std::function<void()> func);
        const std::string dbpath;
        const int maxbattles;
        const int persistfreq;
        const int timeout;
        int persistcounter;
        void verify(int side, int npools, int poolsize);
    };
}

VCMI_LIB_NAMESPACE_END
