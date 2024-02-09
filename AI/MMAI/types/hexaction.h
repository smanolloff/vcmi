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
        MOVE,       // = Move to (defend if current hex)
        AMOVE_TL,   // = Move to (*) + attack:
        AMOVE_TR,   //   . . .  . . . . . 0 1 . . .
        AMOVE_R,    //   1-hex:  . . . . 5 * 2 . . .
        AMOVE_BR,   //  . . . . . . . . . 4 3 . . .
        AMOVE_BL,   //   . . . . . . . . . . . . . .
        AMOVE_L,    //  . . . . . . . . 8 0 1 . . .
        AMOVE_2BL,  //   2-hex (L):  . 7 # * 2 . . .
        AMOVE_2L,   //  . . . . . . . . 6 4 3 . . .
        AMOVE_2TL,  //   . . . . . . . . . . . . . .
        AMOVE_2TR,  //  . . . . . . . . . 0 1 9 . .
        AMOVE_2R,   //   2-hex (R):  . . 5 * # 10. .
        AMOVE_2BR,  //  . . . . . . . . . 4 3 11. .
        SHOOT,      // = shoot at
        count
    };

    static auto EDIR_TO_AMOVE = std::map<BattleHex::EDir, HexAction>{
        {BattleHex::TOP_LEFT,       HexAction::AMOVE_TL},
        {BattleHex::TOP_RIGHT,      HexAction::AMOVE_TR},
        {BattleHex::RIGHT,          HexAction::AMOVE_R},
        {BattleHex::BOTTOM_RIGHT,   HexAction::AMOVE_BR},
        {BattleHex::BOTTOM_LEFT,    HexAction::AMOVE_BL},
        {BattleHex::LEFT,           HexAction::AMOVE_L},
    };

    // for double stacks only, occupiedHex's POV
    static auto EDIR_TO_AMOVE_2 = std::map<BattleHex::EDir, HexAction>{
        {BattleHex::BOTTOM_LEFT,    HexAction::AMOVE_2BL},
        {BattleHex::LEFT,           HexAction::AMOVE_2L},
        {BattleHex::TOP_LEFT,       HexAction::AMOVE_2TL},
        {BattleHex::TOP_RIGHT,      HexAction::AMOVE_2TR},
        {BattleHex::RIGHT,          HexAction::AMOVE_2R},
        {BattleHex::BOTTOM_RIGHT,   HexAction::AMOVE_2BR},
    };


    static auto AMOVE_TO_EDIR = std::map<HexAction, BattleHex::EDir>{
        {HexAction::AMOVE_TL,       BattleHex::TOP_LEFT},
        {HexAction::AMOVE_TR,       BattleHex::TOP_RIGHT},
        {HexAction::AMOVE_R,        BattleHex::RIGHT},
        {HexAction::AMOVE_BR,       BattleHex::BOTTOM_RIGHT},
        {HexAction::AMOVE_BL,       BattleHex::BOTTOM_LEFT},
        {HexAction::AMOVE_L,        BattleHex::LEFT},
        {HexAction::AMOVE_2BL,      BattleHex::BOTTOM_LEFT},
        {HexAction::AMOVE_2L,       BattleHex::LEFT},
        {HexAction::AMOVE_2TL,      BattleHex::TOP_LEFT},
        {HexAction::AMOVE_2TR,      BattleHex::TOP_RIGHT},
        {HexAction::AMOVE_2R,       BattleHex::RIGHT},
        {HexAction::AMOVE_2BR,      BattleHex::BOTTOM_RIGHT},
    };


    // // R/TR/BR or L/TL/BL
    // static auto EDIR_SPECIALS = std::map<BattleSide::Type, std::array<BattleHex::EDir, 3>> {
    //     {BattleSide::ATTACKER, {BattleHex::RIGHT, BattleHex::TOP_RIGHT, BattleHex::BOTTOM_RIGHT}},
    //     {BattleSide::DEFENDER, {BattleHex::LEFT, BattleHex::TOP_LEFT, BattleHex::BOTTOM_LEFT}}
    // };


    static_assert(EI(NonHexAction::count) == Export::N_NONHEX_ACTIONS);
    static_assert(EI(HexAction::count) == Export::N_HEX_ACTIONS);

    constexpr int N_ACTIONS = EI(NonHexAction::count) + EI(HexAction::count)*BF_SIZE;
}
