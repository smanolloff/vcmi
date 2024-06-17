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

#include "BAI/v1/supplementary_data.h"
#include "common.h"

namespace MMAI::BAI::V1 {
    const Schema::V1::Hexes SupplementaryData::getHexes() const {
        ASSERT(battlefield, "getHexes() called when battlefield is null");
        auto res = Schema::V1::Hexes{};

        for (int y=0; y<battlefield->hexes.size(); ++y) {
            auto &hexrow = battlefield->hexes.at(y);
            auto &resrow = res.at(y);
            for (int x=0; x<hexrow.size(); ++x) {
                resrow.at(x) = hexrow.at(x).get();
            }
        }

        return res;
    }

    const Schema::V1::AttackLogs SupplementaryData::getAttackLogs() const {
        auto res = Schema::V1::AttackLogs{};
        res.reserve(attackLogs.size());

        for (auto &al : attackLogs)
            res.push_back(al.get());

        return res;
    }
}
