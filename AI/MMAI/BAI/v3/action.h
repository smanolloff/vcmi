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

#include "./hex.h"
#include "./hexaction.h"
#include "./battlefield.h"

namespace MMAI::BAI::V3 {
    /**
     * Wrapper around Schema::Action
     */
    struct Action {
        static std::unique_ptr<Hex> initHex(const Schema::Action &a, const Battlefield * bf);
        static std::unique_ptr<Hex> initAMoveTargetHex(const Schema::Action &a, const Battlefield * bf);
        static HexAction initHexAction(const Schema::Action &a, const Battlefield * bf);

        Action(const Schema::Action action_, const Battlefield * bf, const std::string color);

        const std::string color;
        const Schema::Action action;
        const std::unique_ptr<Hex> hex;
        const std::unique_ptr<Hex> aMoveTargetHex;
        const HexAction hexaction; // XXX: must come after action

        std::string name() const;
    };
}
