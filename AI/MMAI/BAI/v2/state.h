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

#include "BAI/v1/state.h"

namespace MMAI::BAI::V2 {
    using HexAttribute = Schema::V1::HexAttribute;
    using BS = Schema::BattlefieldState;

    class State : public V1::State {
        using V1::State::State; // inherit parent constructor

        int version() const override { return 2; }

        void encodeHex(V1::Hex* hex) override;
        void verify() override;
    };
}
