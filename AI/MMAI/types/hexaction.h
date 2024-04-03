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
        AMOVE_TR,   // = Move to (*) + attack:
        AMOVE_R,    //  . . . . . . . . . 5 0 . . . .
        AMOVE_BR,   // . 1-hex:  . . . . 4 * 1 . . .
        AMOVE_BL,   //  . . . . . . . . . 3 2 . . . .
        AMOVE_L,    // . . . . . . . . . . . . . . .
        AMOVE_TL,   //  . . . . . . . . . 5 0 6 . . .
        AMOVE_2TR,  // . 2-hex (R):  . . 4 * # 7 . .
        AMOVE_2R,   //  . . . . . . . . . 3 2 8 . . .
        AMOVE_2BR,  // . . . . . . . . . . . . . . .
        AMOVE_2BL,  //  . . . . . . . .11 5 0 . . . .
        AMOVE_2L,   // . 2-hex (L):  .10 # * 1 . . .
        AMOVE_2TL,  //  . . . . . . . . 9 3 2 . . . .
        MOVE,       // = Move to (defend if current hex)
        SHOOT,      // = shoot at
        count
    };

    static auto EDIR_TO_AMOVE = std::map<BattleHex::EDir, HexAction>{
        {BattleHex::TOP_RIGHT,      HexAction::AMOVE_TR},
        {BattleHex::RIGHT,          HexAction::AMOVE_R},
        {BattleHex::BOTTOM_RIGHT,   HexAction::AMOVE_BR},
        {BattleHex::BOTTOM_LEFT,    HexAction::AMOVE_BL},
        {BattleHex::LEFT,           HexAction::AMOVE_L},
        {BattleHex::TOP_LEFT,       HexAction::AMOVE_TL},
    };

    // for double stacks only, occupiedHex's POV
    static auto EDIR_TO_AMOVE_2 = std::map<BattleHex::EDir, HexAction>{
        {BattleHex::TOP_RIGHT,      HexAction::AMOVE_2TR},
        {BattleHex::RIGHT,          HexAction::AMOVE_2R},
        {BattleHex::BOTTOM_RIGHT,   HexAction::AMOVE_2BR},
        {BattleHex::BOTTOM_LEFT,    HexAction::AMOVE_2BL},
        {BattleHex::LEFT,           HexAction::AMOVE_2L},
        {BattleHex::TOP_LEFT,       HexAction::AMOVE_2TL},
    };


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


    // // R/TR/BR or L/TL/BL
    // static auto EDIR_SPECIALS = std::map<BattleSide::Type, std::array<BattleHex::EDir, 3>> {
    //     {BattleSide::ATTACKER, {BattleHex::RIGHT, BattleHex::TOP_RIGHT, BattleHex::BOTTOM_RIGHT}},
    //     {BattleSide::DEFENDER, {BattleHex::LEFT, BattleHex::TOP_LEFT, BattleHex::BOTTOM_LEFT}}
    // };


    static_assert(EI(NonHexAction::count) == Export::N_NONHEX_ACTIONS);
    static_assert(EI(HexAction::count) == Export::N_HEX_ACTIONS);

    constexpr int N_ACTIONS = EI(NonHexAction::count) + EI(HexAction::count)*BF_SIZE;
}
