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

#include "types/hex.h"
#include "export.h"

namespace MMAI {
    using A = Export::Attribute;
    using E = Export::Encoding;

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
        attrs.fill(ATTR_UNSET);
        hexactmask.fill(false);
    }

    int Hex::attr(Export::Attribute a) const { return attrs.at(EI(a)); };
    void Hex::setattr(Export::Attribute a, int value) { attrs.at(EI(a)) = value; };

    bool Hex::isFree() const { return getState() == HexState::FREE; }
    bool Hex::isObstacle() const { return getState() == HexState::OBSTACLE; }
    bool Hex::isOccupied() const { return getState() == HexState::OCCUPIED; }

    int Hex::getX() const { return attrs.at(EI(Export::Attribute::HEX_X_COORD)); }
    int Hex::getY() const { return attrs.at(EI(Export::Attribute::HEX_Y_COORD)); }
    HexState Hex::getState() const { return HexState(attrs.at(EI(A::HEX_STATE))); }

    std::string Hex::name() const {
        return "(" + std::to_string(1 + getX()) + "," + std::to_string(1 + getY()) + ")";
    }

    //
    // Attribute setters
    //

    void Hex::setX(int x) { attrs.at(EI(Export::Attribute::HEX_X_COORD)) = x; }
    void Hex::setY(int y) { attrs.at(EI(Export::Attribute::HEX_Y_COORD)) = y; }

    void Hex::setState(HexState state) {
        attrs.at(EI(A::HEX_STATE)) = EI(state);
    }

    //
    // REACHABLE_BY_*
    //

    void Hex::setReachableByStack(bool isActive, bool isFriendly, int slot, bool value) {
        if (isFriendly) {
            setReachableByFriendlyStack(slot, value);
            if (isActive) setReachableByActiveStack(value);
        } else {
            setReachableByEnemyStack(slot, value);
        }
    }

    void Hex::setReachableByActiveStack(bool value) {
        attrs.at(EI(A::HEX_REACHABLE_BY_ACTIVE_STACK)) = value;
    }

    void Hex::setReachableByFriendlyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0)) = value;
    }

    void Hex::setReachableByEnemyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_REACHABLE_BY_ENEMY_STACK_0)) = value;
    }

    //
    // MELEEABLE_BY_*
    //

    void Hex::setMeleeableByStack(bool isActive, bool isFriendly, int slot, Export::DmgMod mod) {
        if (isFriendly) {
            setMeleeableByFriendlyStack(slot, mod);
            if (isActive) setMeleeableByActiveStack(mod);
        } else {
            setMeleeableByEnemyStack(slot, mod);
        }
    }

    void Hex::setMeleeableByActiveStack(Export::DmgMod value) {
        attrs.at(EI(A::HEX_MELEEABLE_BY_ACTIVE_STACK)) = EI(value);
    }

    void Hex::setMeleeableByFriendlyStack(int slot, Export::DmgMod value) {
        attrs.at(slot + EI(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_0)) = EI(value);
    }

    void Hex::setMeleeableByEnemyStack(int slot, Export::DmgMod value) {
        attrs.at(slot + EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_0)) = EI(value);
    }

    //
    // SHOOTABLE_BY_*
    //

    void Hex::setShootableByStack(bool isActive, bool isFriendly, int slot, Export::DmgMod mod) {
        if (isFriendly) {
            setShootableByFriendlyStack(slot, mod);
            if (isActive) setShootableByActiveStack(mod);
        } else {
            setShootableByEnemyStack(slot, mod);
        }
    }

    void Hex::setShootableByActiveStack(Export::DmgMod value) {
        attrs.at(EI(A::HEX_SHOOTABLE_BY_ACTIVE_STACK)) = EI(value);
    }

    void Hex::setShootableByFriendlyStack(int slot, Export::DmgMod value) {
        attrs.at(slot + EI(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_0)) = EI(value);
    }

    void Hex::setShootableByEnemyStack(int slot, Export::DmgMod value) {
        attrs.at(slot + EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_0)) = EI(value);
    }

    //
    // NEXT_TO_*
    //

    void Hex::setNextToStack(bool isActive, bool isFriendly, int slot, bool value) {
        if (isFriendly) {
            setNextToFriendlyStack(slot, value);
            if (isActive) setNextToActiveStack(value);
        } else {
            setNextToEnemyStack(slot, value);
        }
    }

    void Hex::setNextToActiveStack(bool value) {
        attrs.at(EI(A::HEX_NEXT_TO_ACTIVE_STACK)) = value;
    }

    void Hex::setNextToFriendlyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_NEXT_TO_FRIENDLY_STACK_0)) = value;
    }

    void Hex::setNextToEnemyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_NEXT_TO_ENEMY_STACK_0)) = value;
    }

    void Hex::setCStackAndAttrs(const CStack* cstack_, int qpos) {
        cstack = cstack_;

        setattr(A::STACK_QUANTITY,  cstack->getCount());
        setattr(A::STACK_ATTACK,    cstack->getAttack(false));
        setattr(A::STACK_DEFENSE,   cstack->getDefense(false));
        setattr(A::STACK_SHOTS,     cstack->shots.available());
        setattr(A::STACK_DMG_MIN,   cstack->getMinDamage(false));
        setattr(A::STACK_DMG_MAX,   cstack->getMaxDamage(false));
        setattr(A::STACK_HP,        cstack->getMaxHealth());
        setattr(A::STACK_HP_LEFT,   cstack->getFirstHPleft());
        setattr(A::STACK_SPEED,     cstack->speed());
        setattr(A::STACK_WAITED,    cstack->waitedThisTurn);
        setattr(A::STACK_QUEUE_POS, qpos);

        auto inf = cstack->hasBonusOfType(BonusType::UNLIMITED_RETALIATIONS);

        setattr(A::STACK_RETALIATIONS_LEFT,     inf ? 3 : cstack->counterAttacks.available());
        setattr(A::STACK_SIDE,                  cstack->unitSide());
        setattr(A::STACK_SLOT,                  cstack->unitSlot());
        setattr(A::STACK_CREATURE_TYPE,         cstack->creatureId());
        setattr(A::STACK_AI_VALUE_TENTH,        cstack->creatureId().toCreature()->getAIValue() / 10);
        setattr(A::STACK_IS_ACTIVE,             (qpos == 0));
        setattr(A::STACK_IS_WIDE,               cstack-> doubleWide());
        setattr(A::STACK_FLYING,                cstack->hasBonusOfType(BonusType::FLYING));
        setattr(A::STACK_NO_MELEE_PENALTY,      cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY));
        setattr(A::STACK_TWO_HEX_ATTACK_BREATH, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH));
        setattr(A::STACK_BLOCKS_RETALIATION,    cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION));
        setattr(A::STACK_DEFENSIVE_STANCE,      cstack->hasBonusOfType(BonusType::DEFENSIVE_STANCE));
    }

}
