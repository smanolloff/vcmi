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

#include "battle/BattleHex.h"
#include "common.h"
#include "export.h"

// There is a cyclic dependency if those are placed in action.h:
// action.h -> battlefield.h -> hex.h -> actmask.h -> action.h
namespace MMAI {
    enum class NonHexAction : int {
        RETREAT,
        WAIT,
        count
    };

    using HexAction = MMAI::Export::HexAction;

    static auto AMOVE_TO_EDIR = std::map<HexAction, BattleHex::EDir>{
        {HexAction::AMOVE_TR,       BattleHex::TOP_RIGHT},
        {HexAction::AMOVE_R,        BattleHex::RIGHT},
        {HexAction::AMOVE_BR,       BattleHex::BOTTOM_RIGHT},
        {HexAction::AMOVE_BL,       BattleHex::BOTTOM_LEFT},
        {HexAction::AMOVE_L,        BattleHex::LEFT},
        {HexAction::AMOVE_TL,       BattleHex::TOP_LEFT},
        {HexAction::AMOVE_2TR,      BattleHex::TOP_RIGHT},
        {HexAction::AMOVE_2R,       BattleHex::RIGHT},
        {HexAction::AMOVE_2BR,      BattleHex::BOTTOM_RIGHT},
        {HexAction::AMOVE_2BL,      BattleHex::BOTTOM_LEFT},
        {HexAction::AMOVE_2L,       BattleHex::LEFT},
        {HexAction::AMOVE_2TL,      BattleHex::TOP_LEFT},
    };


    static_assert(EI(NonHexAction::count) == Export::N_NONHEX_ACTIONS);
    static_assert(EI(HexAction::_count) == Export::N_HEX_ACTIONS);

    constexpr int N_ACTIONS = EI(NonHexAction::count) + EI(HexAction::_count)*BF_SIZE;
}
