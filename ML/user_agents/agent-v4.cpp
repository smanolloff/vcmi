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

#include "./agent-v4.h"
#include "AI/MMAI/common.h"
#include "AI/MMAI/schema/v4/types.h"

namespace ML {
    namespace UserAgents {
        std::string AgentV4::getName() { return "UserAgent (v4)"; };
        int AgentV4::getVersion() { return 4; };
        double AgentV4::getValue(const MMAI::Schema::IState * s) { return -666; };

        MMAI::Schema::Action AgentV4::getAction(const MMAI::Schema::IState * s) {
            MMAI::Schema::Action act;

            if (s->version() != 4)
                throw std::runtime_error("Expected version 4, got: " + std::to_string(s->version()));

            auto any = s->getSupplementaryData();
            auto err = MMAI::Schema::AnyCastError(any, typeid(const MMAI::Schema::V4::ISupplementaryData*));
            ASSERT(err.empty(), "anycast for getSumpplementaryData error: " + err);

            auto sup = std::any_cast<const MMAI::Schema::V4::ISupplementaryData*>(any);
            auto side = static_cast<int>(sup->getSide());

            if (steps == 0 && benchmark) {
                t0 = clock();
            }

            steps++;

            if (sup->getType() == MMAI::Schema::V4::ISupplementaryData::Type::ANSI_RENDER) {
                std::cout << sup->getAnsiRender() << "\n";
                // use stored mask from pre-render result
                act = interactive
                    ? promptAction(lastmask)
                    : (actions.empty() ? randomValidAction(lastmask) : recordedAction());

                render = false;
            } else if (!benchmark && !render) {
                logAi->debug("Side: %d", side);
                render = true;
                // store mask of this result for the next action
                lastmask = s->getActionMask();
                act = MMAI::Schema::ACTION_RENDER_ANSI;
            } else if (sup->getIsBattleEnded()) {
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

                if (!benchmark) logGlobal->debug("user-callback battle ended => sending ACTION_RESET");
                act = MMAI::Schema::ACTION_RESET;
            // } else if (false)
            } else {
                render = false;
                act = interactive
                    ? promptAction(s->getActionMask())
                    : (actions.empty() ? randomValidAction(s->getActionMask()) : recordedAction());
            }

            if (verbose && !benchmark) logGlobal->debug("user-callback getAction returning: %d", EI(act));
            return act;
        };


        MMAI::Schema::Action AgentV4::promptAction(const MMAI::Schema::ActionMask &mask) {
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

        MMAI::Schema::Action AgentV4::recordedAction() {
            if (recording_i >= actions.size()) throw std::runtime_error("\n\n*** No more recorded actions in actions.txt ***\n\n");
            return MMAI::Schema::Action(actions[recording_i++]);
        };

        MMAI::Schema::Action AgentV4::randomValidAction(const MMAI::Schema::ActionMask &mask) {
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

        MMAI::Schema::Action AgentV4::firstValidAction(const MMAI::Schema::ActionMask &mask) {
            for (int j = 1; j < mask.size(); j++)
                if (mask[j]) return j;

            return -5;
        }
    }
}
