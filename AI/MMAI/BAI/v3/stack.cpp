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

#include "Global.h"

#include "./stack.h"
#include "./hex.h"
#include "bonuses/BonusCustomTypes.h"
#include "constants/EntityIdentifiers.h"
#include "schema/v3/constants.h"
#include "schema/v3/types.h"

namespace MMAI::BAI::V3 {
    using A = Schema::V3::StackAttribute;

    Stack::Stack(const CStack* cstack_, int id, Queue &q)
    : cstack(cstack_)
    {
        auto id2 = id % MAX_STACKS_PER_SIDE;
        alias = id2 + (id2 < 7 ? '0' : 'A');

        auto [x, y] = Hex::CalcXY(cstack->getPosition());
        auto nomorale = false;
        auto noluck = false;
        auto bonuses = cstack->getAllBonuses(Selector::all, nullptr);
        auto noretal = false;

        for (auto &bonus : *bonuses) {
            switch (bonus->type) {
            break; case BonusType::MORALE:
                addattr(A::MORALE, bonus->val);
            break; case BonusType::LUCK:
                addattr(A::LUCK, bonus->val);
            break; case BonusType::FREE_SHOOTING:
                setattr(A::FREE_SHOOTING, 1);
            break; case BonusType::FLYING:
                setattr(A::FLYING, 1);
            break; case BonusType::SHOOTER:
                setattr(A::SHOOTER, 1);
            // break; case BonusType::CHARGE_IMMUNITY:
            //     setattr(A::CHARGE_IMMUNITY, 1);
            break; case BonusType::ADDITIONAL_ATTACK:
                setattr(A::ADDITIONAL_ATTACK, 1);
            break; case BonusType::NO_MELEE_PENALTY:
                setattr(A::NO_MELEE_PENALTY, 1);
            break; case BonusType::JOUSTING:
                setattr(A::JOUSTING, 1);
            break; case BonusType::HATE:
                // XXX: for dual-hate (where upgraded version is also hated)
                //      we don't have separate attribute
                //      => add only un-upgraded for those (to prevent summing)
                switch(bonus->subtype.as<CreatureID>()) {
                break; case CreatureID::ANGEL: addattr(A::HATES_ANGELS, bonus->val);
                break; case CreatureID::GENIE: addattr(A::HATES_GENIES, bonus->val);
                break; case CreatureID::TITAN: addattr(A::HATES_TITANS, bonus->val);
                break; case CreatureID::EFREETI: addattr(A::HATES_EFREET, bonus->val);
                break; case CreatureID::DEVIL: addattr(A::HATES_DEVILS, bonus->val);
                break; case CreatureID::BLACK_DRAGON: addattr(A::HATES_BLACK_DRAGONS, bonus->val);
                break; case CreatureID::AIR_ELEMENTAL: addattr(A::HATES_AIR_ELEMENTALS, bonus->val);
                break; case CreatureID::EARTH_ELEMENTAL: addattr(A::HATES_EARTH_ELEMENTALS, bonus->val);
                break; case CreatureID::FIRE_ELEMENTAL: addattr(A::HATES_FIRE_ELEMENTALS, bonus->val);
                break; case CreatureID::WATER_ELEMENTAL: addattr(A::HATES_WATER_ELEMENTALS, bonus->val);
                break;
                };
            break; case BonusType::MAGIC_RESISTANCE:
                // XXX: being near a unicorn does NOT not give MAGIC_RESISTSNCE
                //      bonus -- must check neighbouring hexes manually and add
                //      the percentage here -- not worth the effort
                setattr(A::MAGIC_RESISTANCE, bonus->val);
            break; case BonusType::SPELL_AFTER_ATTACK:
                switch(bonus->subtype.as<SpellID>()) {
                break; case SpellID::BLIND:
                       case SpellID::STONE_GAZE:
                       case SpellID::PARALYZE:
                    addattr(A::BLIND_LIKE_ATTACK, bonus->val);
                break; case SpellID::WEAKNESS:
                       case SpellID::DISEASE:
                    addattr(A::WEAKENING_ATTACK, bonus->val);
                break; case SpellID::DISPEL:
                       case SpellID::DISPEL_HELPFUL_SPELLS:
                    addattr(A::DISPELLING_ATTACK, bonus->val);
                break; case SpellID::POISON:
                    addattr(A::POISONOUS_ATTACK, bonus->val);
                break; case SpellID::CURSE:
                    addattr(A::CURSING_ATTACK, bonus->val);
                break; case SpellID::AGE:
                    addattr(A::AGING_ATTACK, bonus->val);
                break; case SpellID::BIND:
                    addattr(A::BINDING_ATTACK, bonus->val);
                break; case SpellID::THUNDERBOLT:
                    addattr(A::LIGHTNING_ATTACK, bonus->val);
                }
            break; case BonusType::SPELL_RESISTANCE_AURA:
                addattr(A::SPELL_RESISTANCE_AURA, bonus->val);
            break; case BonusType::LEVEL_SPELL_IMMUNITY:
                setattr(A::LEVEL_SPELL_IMMUNITY, 1);
                if (bonus->val == 5)
                    addattr(A::MAGIC_RESISTANCE, 100);
            break; case BonusType::TWO_HEX_ATTACK_BREATH:
                setattr(A::TWO_HEX_ATTACK_BREATH, 1);
            break; case BonusType::SPELL_DAMAGE_REDUCTION:
                addattr(A::FIRE_DAMAGE_REDUCTION, bonus->val);
                addattr(A::WATER_DAMAGE_REDUCTION, bonus->val);
                addattr(A::EARTH_DAMAGE_REDUCTION, bonus->val);
                addattr(A::AIR_DAMAGE_REDUCTION, bonus->val);
            break; case BonusType::NO_WALL_PENALTY:
                setattr(A::NO_WALL_PENALTY, 1);
            break; case BonusType::NON_LIVING:
                   case BonusType::UNDEAD:
                   case BonusType::SIEGE_WEAPON:
                setattr(A::NON_LIVING, 1);
                setattr(A::MIND_IMMUNITY, 1); // redundant?
                nomorale = true;
            break; case BonusType::BLOCKS_RETALIATION:
                setattr(A::BLOCKS_RETALIATION, 1);
            break; case BonusType::SPELL_LIKE_ATTACK:
                switch(bonus->subtype.as<SpellID>()) {
                break; case SpellID::FIREBALL: addattr(A::AREA_ATTACK, 1);
                break; case SpellID::DEATH_CLOUD: setattr(A::AREA_ATTACK, 2);
                }
            break; case BonusType::THREE_HEADED_ATTACK:
                setattr(A::THREE_HEADED_ATTACK, 1);
            break; case BonusType::MIND_IMMUNITY:
                setattr(A::MIND_IMMUNITY, 1);
            break; case BonusType::FIRE_SHIELD:
                addattr(A::FIRE_SHIELD, bonus->val);
            break; case BonusType::LIFE_DRAIN:
                setattr(A::LIFE_DRAIN, bonus->val);
            break; case BonusType::DOUBLE_DAMAGE_CHANCE:
                setattr(A::DOUBLE_DAMAGE_CHANCE, 1);
            break; case BonusType::RETURN_AFTER_STRIKE:
                setattr(A::RETURN_AFTER_STRIKE, 1);
            break; case BonusType::ENEMY_DEFENCE_REDUCTION:
                setattr(A::ENEMY_DEFENCE_REDUCTION, bonus->val);
            break; case BonusType::GENERAL_DAMAGE_REDUCTION: {
                auto st = bonus->subtype.as<BonusCustomSubtype>();
                if (st == BonusCustomSubtype::damageTypeAll) {
                    addattr(A::MELEE_DAMAGE_REDUCTION, bonus->val);
                    addattr(A::RANGED_DAMAGE_REDUCTION, bonus->val);
                } else if (st == BonusCustomSubtype::damageTypeRanged) {
                    addattr(A::RANGED_DAMAGE_REDUCTION, bonus->val);
                } else if (st == BonusCustomSubtype::damageTypeMelee) {
                    addattr(A::MELEE_DAMAGE_REDUCTION, bonus->val);
                }
            }
            break; case BonusType::GENERAL_ATTACK_REDUCTION: {
                auto st = bonus->subtype.as<BonusCustomSubtype>();
                if (st == BonusCustomSubtype::damageTypeAll) {
                    addattr(A::MELEE_ATTACK_REDUCTION, bonus->val);
                    addattr(A::RANGED_ATTACK_REDUCTION, bonus->val);
                } else if (st == BonusCustomSubtype::damageTypeRanged) {
                    addattr(A::RANGED_ATTACK_REDUCTION, bonus->val);
                } else if (st == BonusCustomSubtype::damageTypeMelee) {
                    addattr(A::MELEE_ATTACK_REDUCTION, bonus->val);
                }
            }
            break; case BonusType::DEFENSIVE_STANCE:
                setattr(A::DEFENSIVE_STANCE, 1);
            break; case BonusType::ATTACKS_ALL_ADJACENT:
                setattr(A::ATTACKS_ALL_ADJACENT, 1);
            break; case BonusType::NO_DISTANCE_PENALTY:
                setattr(A::NO_DISTANCE_PENALTY, 1);
            break; case BonusType::HYPNOTIZED:
                setattr(A::HYPNOTIZED, bonus->duration == BonusDuration::PERMANENT ? 100 : bonus->turnsRemain);
            break; case BonusType::NO_RETALIATION:
                noretal = true;
            break; case BonusType::MAGIC_MIRROR:
                addattr(A::MAGIC_MIRROR, bonus->val);
            break; case BonusType::ATTACKS_NEAREST_CREATURE:
                setattr(A::ATTACKS_NEAREST_CREATURE, 1);
            // break; case BonusType::FORGETFULL: TODO: check if covered by GENERAL_ATTACK_REDUCTION
            break; case BonusType::NOT_ACTIVE:
                addattr(A::SLEEPING, (bonus->duration & BonusDuration::PERMANENT).any() ? 100 : bonus->turnsRemain);
            break; case BonusType::NO_LUCK:
                noluck = true;
            break; case BonusType::NO_MORALE:
                nomorale = true;
            break; case BonusType::DEATH_STARE:
                setattr(A::DEATH_STARE, 1);
            break; case BonusType::POISON:
                setattr(A::POISON, 1);
            break; case BonusType::ACID_BREATH:
                setattr(A::ACID_ATTACK, bonus->additionalInfo[0]);
            break; case BonusType::REBIRTH:
                setattr(A::REBIRTH, cstack->canCast());
            // break; case BonusType::PERCENTAGE_DAMAGE_BOOST:
            break; case BonusType::SPELL_SCHOOL_IMMUNITY: {
                auto st = bonus->subtype.as<SpellSchool>();
                if (st == SpellSchool::ANY) {
                    addattr(A::AIR_DAMAGE_REDUCTION, 100);
                    addattr(A::WATER_DAMAGE_REDUCTION, 100);
                    addattr(A::FIRE_DAMAGE_REDUCTION, 100);
                    addattr(A::EARTH_DAMAGE_REDUCTION, 100);
                }
                else if (st == SpellSchool::AIR) addattr(A::AIR_DAMAGE_REDUCTION, 100);
                else if (st == SpellSchool::WATER) addattr(A::WATER_DAMAGE_REDUCTION, 100);
                else if (st == SpellSchool::FIRE) addattr(A::FIRE_DAMAGE_REDUCTION, 100);
                else if (st == SpellSchool::EARTH) addattr(A::EARTH_DAMAGE_REDUCTION, 100);
            }
            break;
          }
        }

        if (nomorale) setattr(A::MORALE, 0);
        if (noluck) setattr(A::MORALE, 0);

        auto qit = std::find(q.begin(), q.end(), cstack->unitId());
        auto qpos = (qit == q.end()) ? 100 : qit - q.begin();

        for (int i=0; i<EI(A::_count); ++i) {
            auto a = A(i);
            int &v = attrs.at(i);

            switch (a) {
            break; case A::ID:                  v = id;
            break; case A::Y_COORD:             v = y;
            break; case A::X_COORD:             v = x;
            break; case A::SIDE:                v = cstack->unitSide();
            break; case A::QUANTITY:            v = cstack->getCount();
            break; case A::ATTACK:              v = cstack->getAttack(attr(A::SHOOTER));
            break; case A::DEFENSE:             v = cstack->getDefense(false);
            break; case A::SHOTS:               v = cstack->shots.available();
            break; case A::DMG_MIN:             v = cstack->getMinDamage(attr(A::SHOOTER));
            break; case A::DMG_MAX:             v = cstack->getMaxDamage(attr(A::SHOOTER));
            break; case A::HP:                  v = cstack->getMaxHealth();
            break; case A::HP_LEFT:             v = cstack->getFirstHPleft();
            break; case A::SPEED:               v = cstack->getMovementRange();
            break; case A::WAITED:              v = cstack->waitedThisTurn;
            break; case A::QUEUE_POS:           v = qpos;
            break; case A::RETALIATIONS_LEFT:   v = noretal ? 0 : cstack->counterAttacks.available();
            break; case A::IS_WIDE:             v = cstack->doubleWide();
            break; case A::AI_VALUE:            v = cstack->unitType()->getAIValue();
            }
        }

        finalize();
    }

    const StackAttrs& Stack::getAttrs() const {
        return attrs;
    }

    char Stack::getAlias() const {
        return alias;
    }

    int Stack::getAttr(StackAttribute a) const {
        return attr(a);
    }

    std::string Stack::name() const {
        // return boost::str(boost::format("(%d,%d)") % attr(A::Y_COORD) % attr(A::X_COORD));
        return "(" + std::to_string(attr(A::Y_COORD)) + "," + std::to_string(attr(A::X_COORD)) + ")";
    }

    int Stack::attr(StackAttribute a) const {
        return attrs.at(EI(a));
    };

    void Stack::setattr(StackAttribute a, int value) {
        attrs.at(EI(a)) = value;
    };

    void Stack::addattr(StackAttribute a, int value) {
        attrs.at(EI(a)) += value;
    };

    void Stack::finalize() {
        for (int i=0; i<EI(A::_count); ++i) {
            attrs.at(i) = std::min(attrs.at(i), std::get<3>(STACK_ENCODING.at(i)));
        }
    }
}
