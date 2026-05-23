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

#include "./agent-v15.h"
#include "AI/MMAI/common.h"
#include "AI/MMAI/schema/v15/types.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"

namespace ML {
    namespace UserAgents {
        namespace S15 = MMAI::Schema::V15;
        namespace Graph = S15::Graph;

        std::string AgentV15::getName() { return "USER_AGENT"; };
        int AgentV15::getVersion() { return 15; };
        double AgentV15::getValue(const MMAI::Schema::IState * s) { return -666; };

        MMAI::Schema::Action AgentV15::getAction(const MMAI::Schema::IState * s) {
            MMAI::Schema::Action act;

            if (s->version() != 15)
                throw std::runtime_error("Expected version 15, got: " + std::to_string(s->version()));

            auto any = s->getSupplementaryData();
            auto err = MMAI::Schema::AnyCastError(any, typeid(const S15::ISupplementaryData*));
            MMAI::ASSERT(err.empty(), "anycast for getSumpplementaryData error: " + err);

            const auto * sup = std::any_cast<const S15::ISupplementaryData*>(any);
            const auto * G = sup->getGraph();

            if (steps == 0 && benchmark) {
                t0 = clock();
            }

            steps++;

            auto isEnded = [&sup]
            {
                const auto * global = sup->getGraph()->getNodes(Graph::ElementType::NODE_GLOBAL).at(0);
                return global->rawAttributes().at(EI(Graph::NodeAttributes::Global::BATTLE_WINNER)) != S15::NULL_VALUE_UNENCODED;
            };

            if (sup->getType() == S15::ISupplementaryData::Type::ANSI_RENDER) {
                render = false;
                std::cout << sup->getAnsiRender() << "\n";
            }
            else if (autorender && !benchmark && !render) {
                render = true;
                return MMAI::Schema::ACTION_RENDER_ANSI;
            }

            if (isEnded()) {
                resets++;

                switch (resets % 4) {
                case 0: std::cout << "\r|"; break;
                case 1: std::cout << "\r\\"; break;
                case 2: std::cout << "\r-"; break;
                case 3: std::cout << "\r/"; break;
                default: std::cout << "?"; break;;
                }

                if (resets == 10) {
                    auto s = static_cast<double>(clock() - t0) / CLOCKS_PER_SEC;
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
                    ? promptAction(G)
                    : (actions.empty() ? randomValidAction(G) : recordedAction());
            }

            if (verbose && !benchmark) logGlobal->debug("user-callback getAction returning: %d", EI(act));
            return act;
        };


        MMAI::Schema::Action AgentV15::promptAction(const S15::Graph::IGraph * G) const
        {
            int choice;

            std::unordered_set<int> validChoices;
            for (const auto & id : G->getActiveActionIds())
                validChoices.emplace(id);

            while (true) {
                std::cout << "Enter an integer (blank or 0 for a random valid action): ";

                // Read the user input as a string
                std::string input;
                std::getline(std::cin, input);

                // If the input is empty, treat it as if 0 was entered
                if (input.empty()) {
                    choice = 0;
                    break;
                } else {
                    try {
                        choice = std::stoi(input);

                        if (choice == 0 || validChoices.contains(choice))
                            break;

                        std::cerr << "Invalid input!\n";
                    } catch (const std::invalid_argument& e) {
                        std::cerr << "Invalid input!\n";
                    } catch (const std::out_of_range& e) {
                        std::cerr << "Invalid input!\n";
                    }
                }
            }

            return choice == 0 ? randomValidAction(G) : choice;
        }

        MMAI::Schema::Action AgentV15::recordedAction() {
            if (recording_i >= actions.size()) throw std::runtime_error("\n\n*** No more recorded actions in actions.txt ***\n\n");
            return actions[recording_i++];
        };

        MMAI::Schema::Action AgentV15::randomValidAction(const S15::Graph::IGraph * G) const
        {
            auto activeIds = G->getActiveActionIds();

            if (activeIds.empty()) {
                logAi->info("No valid actions => reset");
                return MMAI::Schema::ACTION_RESET;
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, activeIds.size() - 1);
            int randomIndex = dist(gen);
            const auto id = activeIds[randomIndex];
            return id;
        }

        MMAI::Schema::Action AgentV15::firstValidAction(const S15::Graph::IGraph * G) const
        {
            return G->getActiveActionIds().at(0);
        }
    }
}
