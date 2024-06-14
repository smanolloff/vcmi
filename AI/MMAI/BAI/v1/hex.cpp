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

#include "schema/schema.h"
#include "BAI/v1/hex.h"
#include "BAI/v1/hexactmask.h"

namespace MMAI::BAI::V1 {
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
    }

    const HexAttrs& Hex::getAttrs() const {
        return attrs;
    }

    int Hex::getAttr(HexAttribute a) const {
        return attr(a);
    }

    int Hex::attr(HexAttribute a) const { return attrs.at(EI(a)); };
    void Hex::setattr(HexAttribute a, int value) { attrs.at(EI(a)) = value; };

    bool Hex::isFree() const { return getState() == HexState::FREE; }
    bool Hex::isObstacle() const { return getState() == HexState::OBSTACLE; }
    bool Hex::isOccupied() const { return getState() == HexState::OCCUPIED; }

    int Hex::getX() const { return attrs.at(EI(HexAttribute::HEX_X_COORD)); }
    int Hex::getY() const { return attrs.at(EI(HexAttribute::HEX_Y_COORD)); }
    HexState Hex::getState() const { return HexState(attrs.at(EI(A::HEX_STATE))); }

    std::string Hex::name() const {
        return "(" + std::to_string(getY()) + "," + std::to_string(getX()) + ")";
    }

    //
    // Attribute setters
    //

    void Hex::setPercentCurToStartTotalValue(int percent) { attrs.at(EI(HexAttribute::PERCENT_CUR_TO_START_TOTAL_VALUE)) = percent; }
    void Hex::setX(int x) { attrs.at(EI(HexAttribute::HEX_X_COORD)) = x; }
    void Hex::setY(int y) { attrs.at(EI(HexAttribute::HEX_Y_COORD)) = y; }

    void Hex::setState(HexState state) {
        attrs.at(EI(A::HEX_STATE)) = EI(state);
    }

    //
    // actmasks
    // XXX: those must be manually copied to HEX_ACTION_MASK_FOR_ attribute
    //      prior to serialization
    //

    void Hex::finalizeActionMaskForStack(bool isActive, bool isRight, int slot) {
        if (isActive)
            attrs.at(EI(A::HEX_ACTION_MASK_FOR_ACT_STACK)) = hexactmask.to_ulong();

        if (isRight)
            attrs.at(slot + EI(A::HEX_ACTION_MASK_FOR_R_STACK_0)) = hexactmasks.at(1).at(slot).to_ulong();
        else
            attrs.at(slot + EI(A::HEX_ACTION_MASK_FOR_L_STACK_0)) = hexactmasks.at(0).at(slot).to_ulong();
    }

    void Hex::setActionForStack(bool isActive, bool isRight, int slot, HexAction action) {
        if (isActive)
            hexactmask.set(EI(action));

        hexactmasks.at(isRight).at(slot).set(EI(action));
    }

    //
    // MELEEABLE_BY_*
    //

    void Hex::setMeleeableByStack(bool isActive, bool isRight, int slot, DmgMod mod) {
        if (isActive) setMeleeableByAStack(mod);
        isRight
            ? setMeleeableByRStack(slot, mod)
            : setMeleeableByLStack(slot, mod);
    }

    void Hex::setMeleeableByAStack(DmgMod mod) {
        attrs.at(EI(A::HEX_MELEEABLE_BY_ACT_STACK)) = EI(mod);
    }

    void Hex::setMeleeableByRStack(int slot, DmgMod mod) {
        attrs.at(slot + EI(A::HEX_MELEEABLE_BY_R_STACK_0)) = EI(mod);
    }

    void Hex::setMeleeableByLStack(int slot, DmgMod mod) {
        attrs.at(slot + EI(A::HEX_MELEEABLE_BY_L_STACK_0)) = EI(mod);
    }

    //
    // SHOOTABLE_BY_*
    //

    void Hex::setShootDistanceFromStack(bool isActive, bool isRight, int slot, ShootDistance distance) {
        if (isActive) setShootDistanceFromAStack(distance);
        isRight
            ? setShootDistanceFromRStack(slot, distance)
            : setShootDistanceFromLStack(slot, distance);
    }

    void Hex::setShootDistanceFromAStack(ShootDistance distance) {
        attrs.at(EI(A::HEX_SHOOT_DISTANCE_FROM_ACT_STACK)) = EI(distance);
    }

    void Hex::setShootDistanceFromRStack(int slot, ShootDistance distance) {
        attrs.at(slot + EI(A::HEX_SHOOT_DISTANCE_FROM_R_STACK_0)) = EI(distance);
    }

    void Hex::setShootDistanceFromLStack(int slot, ShootDistance distance) {
        attrs.at(slot + EI(A::HEX_SHOOT_DISTANCE_FROM_L_STACK_0)) = EI(distance);
    }

    //
    // NEXT_TO_*
    //

    void Hex::setMeleeDistanceFromStack(bool isActive, bool isRight, int slot, MeleeDistance distance) {
        if (isActive) setMeleeDistanceFromAStack(distance);
        isRight
            ? setMeleeDistanceFromRStack(slot, distance)
            : setMeleeDistanceFromLStack(slot, distance);
    }

    void Hex::setMeleeDistanceFromAStack(MeleeDistance distance) {
        attrs.at(EI(A::HEX_MELEE_DISTANCE_FROM_ACT_STACK)) = EI(distance);
    }

    void Hex::setMeleeDistanceFromRStack(int slot, MeleeDistance distance) {
        attrs.at(slot + EI(A::HEX_MELEE_DISTANCE_FROM_R_STACK_0)) = EI(distance);
    }

    void Hex::setMeleeDistanceFromLStack(int slot, MeleeDistance distance) {
        attrs.at(slot + EI(A::HEX_MELEE_DISTANCE_FROM_L_STACK_0)) = EI(distance);
    }

    void Hex::setCStackAndAttrs(const CStack* cstack_, int qpos) {
        cstack = cstack_;

        setattr(A::STACK_QUANTITY,  std::min(cstack->getCount(), 1023));
        setattr(A::STACK_ATTACK,    cstack->getAttack(false));
        setattr(A::STACK_DEFENSE,   cstack->getDefense(false));
        setattr(A::STACK_SHOTS,     cstack->shots.available());
        setattr(A::STACK_DMG_MIN,   cstack->getMinDamage(false));
        setattr(A::STACK_DMG_MAX,   cstack->getMaxDamage(false));
        setattr(A::STACK_HP,        cstack->getMaxHealth());
        setattr(A::STACK_HP_LEFT,   cstack->getFirstHPleft());
        setattr(A::STACK_SPEED,     cstack->getMovementRange());
        setattr(A::STACK_WAITED,    cstack->waitedThisTurn);
        setattr(A::STACK_QUEUE_POS, qpos);

        auto inf = cstack->hasBonusOfType(BonusType::UNLIMITED_RETALIATIONS);
        setattr(A::STACK_RETALIATIONS_LEFT,     inf ? 2 : cstack->counterAttacks.available());

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
