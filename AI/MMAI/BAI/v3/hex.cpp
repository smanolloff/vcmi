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

#include "./hex.h"
#include "./hexactmask.h"
#include "schema/v3/constants.h"

namespace MMAI::BAI::V3 {
    using A = Schema::V3::HexAttribute;

    // static
    int Hex::CalcId(const BattleHex &bh) {
        ASSERT(bh.isAvailable(), "Hex unavailable: " + std::to_string(bh.hex));
        return bh.getX()-1 + bh.getY()*BF_XMAX;
    }

    // static
    std::pair<int, int> Hex::CalcXY(const BattleHex &bh) {
        return {bh.getX() - 1, bh.getY()};
    }

    Hex::Hex() {
        attrs.fill(NULL_VALUE_UNENCODED);
    }

    const HexAttrs& Hex::getAttrs() const {
        return attrs;
    }

    int Hex::getAttr(HexAttribute a) const {
        return attr(a);
    }

    int Hex::attr(HexAttribute a) const { return attrs.at(EI(a)); };
    void Hex::setattr(HexAttribute a, int value) {
        attrs.at(EI(a)) = std::min(value, std::get<3>(HEX_ENCODING.at(EI(a))));
    };

    std::string Hex::name() const {
        // return boost::str(boost::format("(%d,%d)") % attr(A::Y_COORD) % attr(A::X_COORD));
        return "(" + std::to_string(attr(A::Y_COORD)) + "," + std::to_string(attr(A::X_COORD)) + ")";
    }

    void Hex::finalizeActionMask() {
        attrs.at(EI(A::ACTION_MASK)) = hexactmask.to_ulong();
    }

    void Hex::permitAction(HexAction action) {
        hexactmask.set(EI(action));
    }
}
