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

namespace UserAgents {
    class Base {
    public:
        Base(bool benchmark_, bool interactive_, bool verbose_, std::vector<int> actions_)
        : benchmark(benchmark_)
        , interactive(interactive_)
        , verbose(verbose_)
        , actions(actions_) {};

        virtual ~Base() = default;
        virtual MMAI::Schema::Action getAction(const MMAI::Schema::IState * s) = 0;
    protected:
        const bool benchmark;  // obsoletes all options below, always picks random actions
        const bool interactive;
        const bool verbose;
        const std::vector<int> actions;
    };
}
