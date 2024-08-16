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

#include "scripted.h"
#include "ML/MLClient.h"
#include <stdexcept>

namespace ML {
    namespace ModelWrappers {
        Scripted::Scripted(std::string keyword)
        : keyword(keyword) {
            auto it = std::find(AIS.begin(), AIS.end(), keyword);
            if (it == AIS.end()) {
                throw std::runtime_error("Unsupported scripted AI keyword: " + keyword);
            }
        };

        std::string Scripted::getName() {
            return keyword;
        };

        // ModelWrappers are a dummy models which have neither version,
        // getAction nor getValue. They are only used as a temporary IModel
        // objects and are used for their name (e.g. StupidAI, BattleAI, etc.)
        // which is then used for creating the corresponding AI.

        int Scripted::getVersion() {
            warn("getVersion", -666);
            return -666;
        };

        int Scripted::getAction(const MMAI::Schema::IState * s) {
            warn("getAction", -666);
            return -666;
        };

        double Scripted::getValue(const MMAI::Schema::IState * s) {
            warn("getValue", -666);
            return -666;
        };

        void Scripted::warn(std::string m, int retval) {
            printf("WARNING: method %s called on a ModelWrapper object; returning %d\n", m.c_str(), retval);
        }
    }
}
