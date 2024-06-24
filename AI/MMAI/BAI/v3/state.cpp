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

#include "battle/CPlayerBattleCallback.h"
#include "battle/IBattleState.h"
#include "networkPacks/PacksForClientBattle.h"
#include "schema/v3/constants.h"
#include "schema/v3/types.h"

#include "./state.h"
#include "./encoder.h"
#include "./hexaction.h"
#include "./supplementary_data.h"
#include "vstd/CLoggerBase.h"

#include <memory>

namespace MMAI::BAI::V3 {
    State::State(const int version__, const std::string colorname_, const CPlayerBattleCallback* battle_)
    : version_(version__)
    , colorname(colorname_)
    , side(battle_->battleGetMySide())
    , initialArmyValues(GeneralInfo::CalcTotalArmyValues(battle_)) {
        bfstate.reserve(Schema::V3::BATTLEFIELD_STATE_SIZE);
        actmask.reserve(Schema::V3::N_ACTIONS);
        // attnmask.reserve(165 * 165);
        battle = battle_;
    }

    void State::onActiveStack(const CStack* astack) {
        int dmgReceived = 0;
        int unitsLost = 0;
        int valueLost = 0;
        int dmgDealt = 0;
        int unitsKilled = 0;
        int valueKilled = 0;

        for (auto &al : attackLogs) {
            // note: in VCMI there is no excess dmg if stack is killed
            if (al->defside == side) {
                dmgReceived += al->dmg;
                unitsLost += al->units;
                valueLost += al->value;
            } else {
                dmgDealt += al->dmg;
                unitsKilled += al->units;
                valueKilled += al->value;
            }
        }

        battlefield = Battlefield::Create(battle, astack, initialArmyValues, isMorale);
        isMorale = false;

        supdata = std::make_unique<SupplementaryData>(
            colorname,
            Side(side),
            dmgDealt,
            dmgReceived,
            unitsLost,
            unitsKilled,
            valueLost,
            valueKilled,
            battlefield.get(),
            attackLogs  // store the logs since last turn
        );

        attackLogs.clear(); // accumulate new logs until next turn
        bfstate.clear();
        actmask.clear();
        // attnmask.clear();

        for (int i=0; i<EI(NonHexAction::count); i++) {
            switch (NonHexAction(i)) {
            break; case NonHexAction::RETREAT: actmask.push_back(true);
            break; case NonHexAction::WAIT: actmask.push_back(battlefield->astack && !battlefield->astack->cstack->waitedThisTurn);
            break; default:
                THROW_FORMAT("Unexpected NonHexAction: %d", i);
            }
        }

        for (auto &hexrow : battlefield->hexes)
            for (auto &hex : hexrow)
                encodeHex(hex.get());

        verify();
    }

    void State::encodeHex(Hex* hex) {
        // Battlefield state
        for (int i=0; i<EI(HexAttribute::_count); ++i) {
            auto a = HexAttribute(i);
            auto v = hex->attrs.at(EI(a));
            Encoder::Encode(a, v, bfstate);
        }

        // Action mask
        for (int m=0; m<hex->hexactmask.size(); ++m) {
            actmask.push_back(hex->hexactmask.test(m));
        }

        // // Attention mask
        // // (not used)
        // if (hex->cstack) {
        //     auto side = hex->attrs.at(EI(HexAttribute::STACK_SIDE));
        //     auto slot = hex->attrs.at(EI(HexAttribute::STACK_SLOT));
        //     for (auto &hexrow1 : bf.hexes) {
        //         for (auto &hex1 : hexrow1) {
        //             if (hex1.hexactmasks.at(side).at(slot).any()) {
        //                 attnmask.push_back(0);
        //             } else {
        //                 attnmask.push_back(std::numeric_limits<float>::lowest());
        //             }
        //         }
        //     }
        // } else {
        //     attnmask.insert(attnmask.end(), 165, std::numeric_limits<float>::lowest());
        // }
    }

    void State::verify() {
        ASSERT(bfstate.size() == BATTLEFIELD_STATE_SIZE, "unexpected bfstate.size(): " + std::to_string(bfstate.size()));
        ASSERT(actmask.size() == N_ACTIONS, "unexpected actmask.size(): " + std::to_string(actmask.size()));
        // ASSERT(attnmask.size() == 165*165, "unexpected attnmask.size(): " + std::to_string(attnmask.size()));
    }

    void State::onBattleStacksAttacked(const std::vector<BattleStackAttacked> &bsa) {
        for(auto & elem : bsa) {
            auto defender = battle->battleGetStackByID(elem.stackAttacked, false);
            auto attacker = battle->battleGetStackByID(elem.attackerID, false);

            ASSERT(defender, "defender cannot be NULL");
            // logAi->debug("Attack: %s -> %s (%d dmg, %d died)", attacker->getName(), defender->getName(), elem.damageAmount, elem.killedAmount);

            attackLogs.push_back(std::make_shared<AttackLog>(
                // XXX: attacker can be NULL when an effect does dmg (eg. Acid)
                attacker ? attacker->unitSlot() : SlotID(-1),
                defender->unitSlot(),
                defender->unitSide(),
                elem.damageAmount,
                elem.killedAmount,
                elem.killedAmount * defender->unitType()->getAIValue()
            ));
        }
    }

    void State::onBattleTriggerEffect(const BattleTriggerEffect &bte) {
        if (static_cast<BonusType>(bte.effect) != BonusType::MORALE)
            return;

        auto stack = battle->battleGetStackByID(bte.stackID);
        isMorale = stack->unitSide() == side;
    }

    void State::onBattleEnd(const BattleResult *br) {
        onActiveStack(nullptr);
        supdata->ended = true;
        supdata->victory = (br->winner == battle->battleGetMySide());
    }
};
