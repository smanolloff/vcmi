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

#include "./battlefield.h"
#include "battle/BattleHex.h"
#include "battle/CPlayerBattleCallback.h"
#include "schema/v3/types.h"
#include "./attack_log.h"

namespace MMAI::BAI::V3 {
    class Stats : public Schema::V3::IStats {
    public:
        Stats(const Battlefield* bf)
        : initialArmyValueLeft(std::get<0>(bf->info->initialArmyValues))
        , initialArmyValueRight(std::get<1>(bf->info->initialArmyValues))
        , currentArmyValueLeft(std::get<0>(bf->info->currentArmyValues))
        , currentArmyValueRight(std::get<1>(bf->info->currentArmyValues)) {}

        int getInitialArmyValueLeft() const override { return initialArmyValueLeft; }
        int getInitialArmyValueRight() const override { return initialArmyValueRight; }
        int getCurrentArmyValueLeft() const override { return currentArmyValueLeft; }
        int getCurrentArmyValueRight() const override { return currentArmyValueRight; }

        const int initialArmyValueLeft;
        const int initialArmyValueRight;
        const int currentArmyValueLeft;
        const int currentArmyValueRight;
    };

    class SupplementaryData : public Schema::V3::ISupplementaryData {
    public:
        SupplementaryData() = delete;

        // Called on activeStack (complete battlefield info)
        SupplementaryData(
            std::string colorname_,
            Side side_,
            int dmgDealt_,
            int dmgReceived_,
            int unitsLost_,
            int unitsKilled_,
            int valueLost_,
            int valueKilled_,
            const Battlefield* battlefield_,
            std::vector<std::shared_ptr<AttackLog>> attackLogs_
        ) : colorname(colorname_),
            side(side_),
            dmgDealt(dmgDealt_),
            dmgReceived(dmgReceived_),
            unitsLost(unitsLost_),
            unitsKilled(unitsKilled_),
            valueLost(valueLost_),
            valueKilled(valueKilled_),
            battlefield(battlefield_),
            stats(std::make_unique<Stats>(battlefield_)),
            attackLogs(attackLogs_) {};

        // impl ISupplementaryData
        Type getType() const override { return type; };
        Side getSide() const override { return side; };
        std::string getColor() const override { return colorname; };
        ErrorCode getErrorCode() const override { return errcode; }; // TODO
        int getDmgDealt() const override { return dmgDealt; };
        int getDmgReceived() const override { return dmgReceived; };
        int getUnitsLost() const override { return unitsLost;  };
        int getUnitsKilled() const override { return unitsKilled; };
        int getValueLost() const override { return valueLost; };
        int getValueKilled() const override { return valueKilled; };
        bool getIsBattleEnded() const override { return ended; };
        bool getIsVictorious() const override { return victory; };

        const Schema::V3::Hexes getHexes() const override;
        const Schema::V3::Stacks getStacks() const override;
        const Schema::V3::AttackLogs getAttackLogs() const override;
        const Schema::V3::IStats* getStats() const override { return stats.get(); }
        const std::string getAnsiRender() const override { return ansiRender; }

        const std::string colorname;
        const Side side;
        const int dmgDealt;
        const int dmgReceived;
        const int unitsLost;
        const int unitsKilled;
        const int valueLost;
        const int valueKilled;
        const Battlefield* battlefield;
        const std::unique_ptr<Stats> stats;
        const std::vector<std::shared_ptr<AttackLog>> attackLogs;

        // Optionally modified (on battlEnd only)
        bool ended = false;
        bool victory = false;

        // Optionally modified (during activeStack if action was invalid)
        ErrorCode errcode = ErrorCode::OK;

        // Optionally modified (during activeStack if action was RENDER)
        Type type = Type::REGULAR;
        std::string ansiRender = "";
    };
}
