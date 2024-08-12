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

#include "./agent-v3.h"
#include "AI/MMAI/common.h"
#include "AI/MMAI/schema/v3/types.h"

namespace UserAgents {
    MMAI::Schema::Action AgentV3::getAction(const MMAI::Schema::IState * s) {
        MMAI::Schema::Action act;

        // Support for other versions can be implemented if needed
        if (s->version() != 3)
            throw std::runtime_error("Expected version 3, got: " + std::to_string(s->version()));

        auto any = s->getSupplementaryData();
        ASSERT(any.has_value(), "supdata is empty");
        auto &t = typeid(const MMAI::Schema::V3::ISupplementaryData*);

        ASSERT(any.type() == t, boost::str(
            boost::format("Bad std::any payload type from getSupplementaryData(): want: %s/%u, have: %s/%u") \
            % boost::core::demangle(t.name()) % t.hash_code() \
            % boost::core::demangle(any.type().name()) % any.type().hash_code()
        ));

        auto sup = std::any_cast<const MMAI::Schema::V3::ISupplementaryData*>(any);
        auto side = static_cast<int>(sup->getSide());

        if (steps == 0 && benchmark) {
            t0 = clock();
            benchside = side;
        }

        steps++;

        if (sup->getType() == MMAI::Schema::V3::ISupplementaryData::Type::ANSI_RENDER) {
            std::cout << sup->getAnsiRender() << "\n";
            // use stored mask from pre-render result
            act = interactive
                ? promptAction(lastmasks.at(side))
                : (actions.empty() ? randomValidAction(lastmasks.at(side)) : recordedAction());

            renders.at(side) = false;
        } else if (!benchmark && !renders.at(side)) {
            logAi->debug("Side: %d", side);
            renders.at(side) = true;
            // store mask of this result for the next action
            lastmasks.at(side) = s->getActionMask();
            act = MMAI::Schema::ACTION_RENDER_ANSI;
        } else if (sup->getIsBattleEnded()) {
            if (side == benchside) {
                resets++;

                switch (resets % 4) {
                case 0: printf("\r|"); break;
                case 1: printf("\r\\"); break;
                case 2: printf("\r-"); break;
                case 3: printf("\r/"); break;
                }

                if (resets == 10) {
                    auto s = double(clock() - t0) / CLOCKS_PER_SEC;
                    printf("  steps/s: %-6.0f resets/s: %-6.2f\n", steps/s, resets/s);
                    resets = 0;
                    steps = 0;
                    t0 = clock();
                }

                std::cout.flush();
            }

            if (!benchmark) logGlobal->debug("user-callback battle ended => sending ACTION_RESET");
            act = MMAI::Schema::ACTION_RESET;
        // } else if (false)
        } else {
            renders.at(side) = false;
            act = interactive
                ? promptAction(s->getActionMask())
                : (actions.empty() ? randomValidAction(s->getActionMask()) : recordedAction());
        }

        if (verbose && !benchmark) logGlobal->debug("user-callback getAction returning: %d", EI(act));
        return act;
    };


    MMAI::Schema::Action AgentV3::promptAction(const MMAI::Schema::ActionMask &mask) {
        int num;

        while (true) {
            std::cout << "Enter an integer (blank or 0 for a random valid action): ";

            // Read the user input as a string
            std::string input;
            std::getline(std::cin, input);

            // If the input is empty, treat it as if 0 was entered
            if (input.empty()) {
                num = 0;
                break;
            } else {
                try {
                    num = std::stoi(input);
                    if (num >= 0)
                        break;
                    else
                        std::cerr << "Invalid input!\n";
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Invalid input!\n";
                } catch (const std::out_of_range& e) {
                    std::cerr << "Invalid input!\n";
                }
            }
        }

        return num == 0 ? randomValidAction(mask) : MMAI::Schema::Action(num);
    }

    MMAI::Schema::Action AgentV3::recordedAction() {
        if (recording_i >= actions.size()) throw std::runtime_error("\n\n*** No more recorded actions in actions.txt ***\n\n");
        return MMAI::Schema::Action(actions[recording_i++]);
    };

    MMAI::Schema::Action AgentV3::randomValidAction(const MMAI::Schema::ActionMask &mask) {
        auto validActions = std::vector<MMAI::Schema::Action>{};

        for (int j = 1; j < mask.size(); j++) {
            if (mask[j])
                validActions.push_back(j);
        }

        if (validActions.empty()) {
            logAi->info("No valid actions => reset");
            return MMAI::Schema::ACTION_RESET;
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, validActions.size() - 1);
        int randomIndex = dist(gen);
        return validActions[randomIndex];
    }

    MMAI::Schema::Action AgentV3::firstValidAction(const MMAI::Schema::ActionMask &mask) {
        for (int j = 1; j < mask.size(); j++)
            if (mask[j]) return j;

        return -5;
    }
}
