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

#include "AI/MMAI/schema/base.h"

namespace ML {
    namespace UserAgents {
        class Base : public MMAI::Schema::IModel {
        public:
            Base(bool benchmark_, bool interactive_, bool autorender_, bool verbose_, std::vector<int> actions_)
            : benchmark(benchmark_)
            , interactive(interactive_)
            , autorender(autorender_)
            , verbose(verbose_)
            , actions(actions_) {};

            MMAI::Schema::ModelType getType() override { return MMAI::Schema::ModelType::USER; };
            std::string getName() override { return ""; };
            int getVersion() override { return 0; };
            int getAction(const MMAI::Schema::IState * s) override { return 0; };
            double getValue(const MMAI::Schema::IState * s) override { return 0; };
            MMAI::Schema::Side getSide() override { return MMAI::Schema::Side::BOTH; };
        protected:
            const bool autorender;
            const bool benchmark;  // obsoletes all options below, always picks random actions
            const bool interactive;
            const bool verbose;
            const std::vector<int> actions;
        };
    }
}
