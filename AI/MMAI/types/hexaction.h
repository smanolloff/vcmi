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

    enum class HexAction : int {
        MOVE,       // = defend if current hex
        SHOOT,      // = shoot at this hex
        MELEE_TL,   // = Melee attacks at this hex from direction (TL=0):
        MELEE_TR,   //  . . . . . . . . . . . . . . . . . . . .
        MELEE_R,    //   . . . . . 0 1 . . . . . . . 0 0 1 1 .
        MELEE_BR,   //   1-hex: . 5 * 2 .  2-hex: . 5 5 * 2 2 .
        MELEE_BL,   //   . . . . . 4 3 . . . . . . . 4 4 3 3 .
        MELEE_L,    //  . . . . . . . . . . . . . . . . . . . .
        MELEE_T,    //   . . . . . . . . . . 7 7 . . . . . . .
        MELEE_B,    //   2-hex (special): . . * . . . . . . . .
        count       //   . . . . . . . . . . 8 8  . . . . . .
    };


    static auto EDIR_TO_MELEE = std::map<BattleHex::EDir, HexAction>{
        {BattleHex::TOP_LEFT,       HexAction::MELEE_TL},
        {BattleHex::TOP_RIGHT,      HexAction::MELEE_TR},
        {BattleHex::RIGHT,          HexAction::MELEE_R},
        {BattleHex::BOTTOM_RIGHT,   HexAction::MELEE_BR},
        {BattleHex::BOTTOM_LEFT,    HexAction::MELEE_BL},
        {BattleHex::LEFT,           HexAction::MELEE_L},
    };

    static auto MELEE_TO_EDIR = std::map<HexAction, BattleHex::EDir>{
        {HexAction::MELEE_TL,   BattleHex::TOP_LEFT},
        {HexAction::MELEE_TR,   BattleHex::TOP_RIGHT},
        {HexAction::MELEE_R,    BattleHex::RIGHT},
        {HexAction::MELEE_BR,   BattleHex::BOTTOM_RIGHT},
        {HexAction::MELEE_BL,   BattleHex::BOTTOM_LEFT},
        {HexAction::MELEE_L,    BattleHex::LEFT},
    };

    // R/TR/BR or L/TL/BL
    static auto EDIR_SPECIALS = std::map<BattleSide::Type, std::array<BattleHex::EDir, 3>> {
        {BattleSide::ATTACKER, {BattleHex::RIGHT, BattleHex::TOP_RIGHT, BattleHex::BOTTOM_RIGHT}},
        {BattleSide::DEFENDER, {BattleHex::LEFT, BattleHex::TOP_LEFT, BattleHex::BOTTOM_LEFT}}
    };


    static_assert(EI(NonHexAction::count) == Export::N_NONHEX_ACTIONS);
    static_assert(EI(HexAction::count) == Export::N_HEX_ACTIONS);

    constexpr int N_ACTIONS = EI(NonHexAction::count) + EI(HexAction::count)*BF_SIZE;
}
