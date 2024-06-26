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

#include "schema/v3/types.h"
#include "./hexactmask.h"
#include "./stack.h"

namespace MMAI::BAI::V3 {
    using namespace Schema::V3;

    /**
     * A wrapper around BattleHex. Differences:
     *
     * x is 0..14     (instead of 0..16),
     * id is 0..164  (instead of 0..177)
     */
    class Hex : public Schema::V3::IHex {
    public:
        static int CalcId(const BattleHex &bh);
        static std::pair<int, int> CalcXY(const BattleHex &bh);

        Hex();

        // IHex impl
        const HexAttrs& getAttrs() const override;
        int getAttr(HexAttribute a) const override;

        BattleHex bhex;
        std::shared_ptr<const Stack> stack = nullptr;
        HexAttrs attrs;
        HexActMask hexactmask; // for active stack only

        std::string name() const;
        int attr(HexAttribute a) const;
        void setattr(HexAttribute a, int value);
        void permitAction(HexAction action);
        void finalizeActionMask();
    };
}
