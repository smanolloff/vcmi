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

#include "./base.h"
#include "schema/v15/graph.h"

namespace ML {
    namespace UserAgents {
        class AgentV15 : public Base {
        public:
            using Base::Base;

            std::string getName() override;
            int getVersion() override;
            int getAction(const MMAI::Schema::IState * s) override;
            double getValue(const MMAI::Schema::IState * s) override;
        private:
            uint64_t steps = 0;
            uint64_t resets = 0;
            clock_t t0 = 0;
            bool render = false;
            int recording_i = 0;

            MMAI::Schema::Action promptAction(const MMAI::Schema::V15::Graph::IGraph * G) const;
            MMAI::Schema::Action recordedAction();
            MMAI::Schema::Action randomValidAction(const MMAI::Schema::V15::Graph::IGraph * G) const;
            MMAI::Schema::Action firstValidAction(const MMAI::Schema::V15::Graph::IGraph * G) const;
        };
    }
}
