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

#include "CStack.h"
#include "battle/BattleHex.h"
#include "export.h"
#include "hexactmask.h"
#include "hexstate.h"

namespace MMAI {
    using HexAttrs = std::array<int, EI(Export::Attribute::_count)>;
    constexpr int ATTR_UNSET = -1;

    /**
     * A wrapper around BattleHex. Differences:
     *
     * x is 0..14     (instead of 0..16),
     * id is 0..164  (instead of 0..177)
     */
    struct Hex {
        static int CalcId(const BattleHex &bh);
        static std::pair<int, int> CalcXY(const BattleHex &bh);

        Hex();

        BattleHex bhex;

        const CStack * cstack;
        HexAttrs attrs;
        HexActMask hexactmask;

        std::string name() const;

        int attr(Export::Attribute a) const;

        bool isFree() const;
        bool isObstacle() const;
        bool isOccupied() const;

        int getX() const;
        int getY() const;
        HexState getState() const;

        void setX(int x);
        void setY(int x);
        void setState(HexState state);
        void setReachableByActiveStack(bool value);
        void setReachableByFriendlyStack(int slot, bool value);
        void setReachableByEnemyStack(int slot, bool value);
        void setMeleeableByActiveStack(Export::DmgMod value);
        void setMeleeableByEnemyStack(int slot, Export::DmgMod value);
        void setShootableByActiveStack(Export::DmgMod value);
        void setShootableByEnemyStack(int slot, Export::DmgMod value);
        void setNextToFriendlyStack(int slot, bool value);
        void setNextToEnemyStack(int slot, bool value);
        void setCStackAndAttrs(const CStack* cstack_, int qpos);

        // void initOccupyingStack()
    };
}
