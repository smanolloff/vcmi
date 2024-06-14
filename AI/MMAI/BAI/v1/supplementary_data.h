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

#include "BAI/v1/battlefield.h"
#include "battle/BattleHex.h"
#include "battle/CPlayerBattleCallback.h"
#include "schema/v1/types.h"
#include "BAI/v1/attack_log.h"

namespace MMAI::BAI::V1 {
    class SupplementaryData : public Schema::V1::ISupplementaryData {
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
            int side0ArmyValue_,
            int side1ArmyValue_,
            Battlefield* battlefield_,
            std::vector<std::shared_ptr<AttackLog>> attackLogs_
        ) : colorname(colorname_),
            side(side_),
            dmgDealt(dmgDealt_),
            dmgReceived(dmgReceived_),
            unitsLost(unitsLost_),
            unitsKilled(unitsKilled_),
            valueLost(valueLost_),
            valueKilled(valueKilled_),
            side0ArmyValue(side0ArmyValue_),
            side1ArmyValue(side1ArmyValue_),
            battlefield(battlefield_),
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
        int getSide0ArmyValue() const override { return side0ArmyValue; };
        int getSide1ArmyValue() const override { return side1ArmyValue; };
        bool getIsBattleEnded() const override { return ended; };
        bool getIsVictorious() const override { return victory; };

        const Schema::V1::Hexes getHexes() const override;
        const Schema::V1::AttackLogs getAttackLogs() const override;
        const std::string getAnsiRender() const override { return ansiRender; }

        const std::string colorname;
        const Side side;
        const int dmgDealt;
        const int dmgReceived;
        const int unitsLost;
        const int unitsKilled;
        const int valueLost;
        const int valueKilled;
        const int side0ArmyValue;
        const int side1ArmyValue;
        const Battlefield* battlefield;
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
