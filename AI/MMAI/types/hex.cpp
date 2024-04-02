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

//
// Compile-time assertions for:
//
// 1. The proper position of the given Attribute within the HexEconding[] array
//      (e.g. that Attribute::HEX_STATE (=2) is found at HexEconding[3*2])
//
// 2. The proper encoding scheme for a given attribute
//      (e.g. that Atribute::HexState has a CATEGORICAL encoding with 4 values)
//
// They gurantee the code logic corresponds to the definitions in export.h
//
#define ASSERT_ENCODING(attr, type, size) \
    static_assert(attr == Export::Attribute(Export::HEX_ENCODING[3*EI(attr)])); \
    static_assert(EI(type) == Export::HEX_ENCODING[1 + 3*EI(attr)]); \
    static_assert(size == Export::HEX_ENCODING[2 + 3*EI(attr)]);

#define ASSERT_7STACK_ENCODINGS(attr, type, size) \
    ASSERT_ENCODING(A(EI(attr)), type, size) \
    ASSERT_ENCODING(A(EI(attr)+1), type, size) \
    ASSERT_ENCODING(A(EI(attr)+2), type, size) \
    ASSERT_ENCODING(A(EI(attr)+3), type, size) \
    ASSERT_ENCODING(A(EI(attr)+4), type, size) \
    ASSERT_ENCODING(A(EI(attr)+5), type, size) \
    ASSERT_ENCODING(A(EI(attr)+6), type, size);

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

    ASSERT_ENCODING(A::HEX_STATE, E::CATEGORICAL, EI(HexState::count));
    void Hex::setState(HexState state) {
        attrs.at(EI(A::HEX_STATE)) = EI(state);
    }

    ASSERT_7STACK_ENCODINGS(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0, E::CATEGORICAL, 2);
    void Hex::setReachableByFriendlyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0)) = value;
    }

    ASSERT_7STACK_ENCODINGS(A::HEX_REACHABLE_BY_ENEMY_STACK_0, E::CATEGORICAL, 2);
    void Hex::setReachableByEnemyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_REACHABLE_BY_ENEMY_STACK_0)) = value;
    }

    ASSERT_7STACK_ENCODINGS(A::HEX_NEXT_TO_FRIENDLY_STACK_0, E::CATEGORICAL, 2);
    void Hex::setNextToFriendlyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_NEXT_TO_FRIENDLY_STACK_0)) = value;
    }

    ASSERT_7STACK_ENCODINGS(A::HEX_NEXT_TO_ENEMY_STACK_0, E::CATEGORICAL, 2);
    void Hex::setNextToEnemyStack(int slot, bool value) {
        attrs.at(slot + EI(A::HEX_NEXT_TO_ENEMY_STACK_0)) = value;
    }

    ASSERT_7STACK_ENCODINGS(A::HEX_MELEEABLE_BY_ENEMY_STACK_0, E::NUMERIC, EI(Export::DmgMod::_count)); // false, half, full
    void Hex::setMeleeableByEnemyStack(int slot, Export::DmgMod value) {
        attrs.at(slot + EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_0)) = EI(value);
    }

    ASSERT_7STACK_ENCODINGS(A::HEX_SHOOTABLE_BY_ENEMY_STACK_0, E::NUMERIC, EI(Export::DmgMod::_count)); // false, half, full
    void Hex::setShootableByEnemyStack(int slot, Export::DmgMod value) {
        attrs.at(slot + EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_0)) = EI(value);
    }

    ASSERT_ENCODING(A::HEX_MELEEABLE_BY_ACTIVE_STACK, E::NUMERIC, EI(Export::DmgMod::_count)); // false, half, full
    void Hex::setMeleeableByActiveStack(Export::DmgMod value) {
        attrs.at(EI(A::HEX_MELEEABLE_BY_ACTIVE_STACK)) = EI(value);
    }

    ASSERT_ENCODING(A::HEX_SHOOTABLE_BY_ACTIVE_STACK, E::NUMERIC, EI(Export::DmgMod::_count)); // false, half, full
    void Hex::setShootableByActiveStack(Export::DmgMod value) {
        attrs.at(EI(A::HEX_SHOOTABLE_BY_ACTIVE_STACK)) = EI(value);
    }

    void Hex::setCStackAndAttrs(const CStack* cstack_, int qpos) {
        cstack = cstack_;

        ASSERT_ENCODING(A::STACK_QUANTITY, E::NUMERIC_SQRT, 32);
        attrs.at(EI(A::STACK_QUANTITY)) = cstack->getCount();

        ASSERT_ENCODING(A::STACK_ATTACK, E::NUMERIC_SQRT, 8);
        attrs.at(EI(A::STACK_ATTACK)) = cstack->getAttack(false);

        ASSERT_ENCODING(A::STACK_DEFENSE, E::NUMERIC_SQRT, 8);
        attrs.at(EI(A::STACK_DEFENSE)) = cstack->getDefense(false);

        ASSERT_ENCODING(A::STACK_SHOTS, E::NUMERIC_SQRT, 5);
        attrs.at(EI(A::STACK_SHOTS)) = cstack->shots.available();

        ASSERT_ENCODING(A::STACK_DMG_MIN, E::NUMERIC_SQRT, 8);
        attrs.at(EI(A::STACK_DMG_MIN)) = cstack->getMinDamage(false);

        ASSERT_ENCODING(A::STACK_DMG_MAX, E::NUMERIC_SQRT, 8);
        attrs.at(EI(A::STACK_DMG_MAX)) = cstack->getMaxDamage(false);

        ASSERT_ENCODING(A::STACK_HP, E::NUMERIC_SQRT, 32);
        attrs.at(EI(A::STACK_HP)) = cstack->getMaxHealth();

        ASSERT_ENCODING(A::STACK_HP_LEFT, E::NUMERIC_SQRT, 32);
        attrs.at(EI(A::STACK_HP_LEFT)) = cstack->getFirstHPleft();

        ASSERT_ENCODING(A::STACK_SPEED, E::NUMERIC, 20);
        attrs.at(EI(A::STACK_SPEED)) = cstack->speed();

        ASSERT_ENCODING(A::STACK_WAITED, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_WAITED)) = cstack->waitedThisTurn;

        ASSERT_ENCODING(A::STACK_QUEUE_POS, E::NUMERIC, 14);
        attrs.at(EI(A::STACK_QUEUE_POS)) = qpos;

        ASSERT_ENCODING(A::STACK_RETALIATIONS_LEFT, E::NUMERIC, 3);
        auto inf = cstack->hasBonusOfType(BonusType::UNLIMITED_RETALIATIONS);
        attrs.at(EI(A::STACK_RETALIATIONS_LEFT)) = inf ? 3 : cstack->counterAttacks.available();

        ASSERT_ENCODING(A::STACK_SIDE, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_SIDE)) = cstack->unitSide();

        ASSERT_ENCODING(A::STACK_SLOT, E::CATEGORICAL, 6);
        attrs.at(EI(A::STACK_SLOT)) = cstack->unitSlot();

        ASSERT_ENCODING(A::STACK_CREATURE_TYPE, E::CATEGORICAL, 150);
        attrs.at(EI(A::STACK_CREATURE_TYPE)) = cstack->creatureId();

        ASSERT_ENCODING(A::STACK_AI_VALUE_TENTH, E::NUMERIC_SQRT, 63);
        attrs.at(EI(A::STACK_AI_VALUE_TENTH)) = cstack->creatureId().toCreature()->getAIValue() / 10;

        ASSERT_ENCODING(A::STACK_IS_ACTIVE, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_IS_ACTIVE)) = (qpos == 0);

        ASSERT_ENCODING(A::STACK_FLYING, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_FLYING)) = cstack->hasBonusOfType(BonusType::FLYING);

        ASSERT_ENCODING(A::STACK_NO_MELEE_PENALTY, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_NO_MELEE_PENALTY)) = cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY);

        ASSERT_ENCODING(A::STACK_TWO_HEX_ATTACK_BREATH, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_TWO_HEX_ATTACK_BREATH)) = cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH);

        ASSERT_ENCODING(A::STACK_BLOCKS_RETALIATION, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_BLOCKS_RETALIATION)) = cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION);

        ASSERT_ENCODING(A::STACK_DEFENSIVE_STANCE, E::CATEGORICAL, 2);
        attrs.at(EI(A::STACK_DEFENSIVE_STANCE)) = cstack->hasBonusOfType(BonusType::DEFENSIVE_STANCE);
    }

}
