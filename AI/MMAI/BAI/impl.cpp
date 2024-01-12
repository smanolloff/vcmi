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

#include "BAI.h"

// Contains boilerplate related implementations of virtual methods

namespace MMAI {
    void BAI::actionStarted(const BattleAction &action) {
        debug("*** actionStarted ***");
    }

    void BAI::yourTacticPhase(int distance) {
        cb->battleMakeTacticAction(BattleAction::makeEndOFTacticPhase(cb->battleGetTacticsSide()));
    }

    void BAI::battleAttack(const BattleAttack *ba) {
        debug("*** battleAttack ***");
    }

    void BAI::battleNewRoundFirst(int round) {
        debug("*** battleNewRoundFirst ***");
    }

    void BAI::battleNewRound(int round) {
        debug("*** battleNewRound ***");
    }

    void BAI::battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) {
        debug("*** battleStackMoved ***");
    }

    void BAI::battleSpellCast(const BattleSpellCast *sc) {
        debug("*** battleSpellCast ***");
    }

    void BAI::battleStacksEffectsSet(const SetStackEffect & sse) {
        debug("*** battleStacksEffectsSet ***");
    }

    void BAI::battleCatapultAttacked(const CatapultAttack & ca) {
        debug("*** battleCatapultAttacked ***");
    }

    void BAI::battleUnitsChanged(const std::vector<UnitChanges> &units) {
        debug("*** battleUnitsChanged ***");
    }

    // NOTE: battleLogMessage is not called for non-player interfaces :(
    // void BAI::battleLogMessage(const std::vector<MetaString> &lines) {
    //   debug("*** battleLogMessage ***");
    //
    //   for(const auto & line : lines) {
    //     std::string formatted = line.toString();
    //     boost::algorithm::trim(formatted);
    //     debug(formatted);
    //   }
    // }
}
