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

namespace UserAgents {
    class AgentV1 : public Base {
    public:
        using Base::Base;
        MMAI::Schema::Action getAction(const MMAI::Schema::IState * s) override;
    private:
        unsigned long steps = 0;
        unsigned long resets = 0;
        int benchside = -1;
        clock_t t0 = 0;
        std::array<bool, 2> renders = {false, false};
        std::array<MMAI::Schema::ActionMask, 2> lastmasks = {};
        int recording_i = 0;

        MMAI::Schema::Action promptAction(const MMAI::Schema::ActionMask &mask);
        MMAI::Schema::Action recordedAction();
        MMAI::Schema::Action randomValidAction(const MMAI::Schema::ActionMask &mask);
        MMAI::Schema::Action firstValidAction(const MMAI::Schema::ActionMask &mask);
    };
}
