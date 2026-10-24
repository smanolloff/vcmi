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

#include "function.h"

namespace ML {
    namespace ModelWrappers {
        Function::Function(
            int version,
            std::string name,
            MMAI::Schema::Side side,
            std::function<int(const MMAI::Schema::IState*)> f_getAction,
            std::function<double(const MMAI::Schema::IState*)> f_getValue
        ) : version(version)
          , name(name)
          , side(side)
          , f_getAction(f_getAction)
          , f_getValue(f_getValue) {};

        MMAI::Schema::ModelType Function::getType() {
            return MMAI::Schema::ModelType::USER;
        };

        std::string Function::getName() {
            return name;
        }

        int Function::getVersion() {
            return version;
        }

        MMAI::Schema::Side Function::getSide() {
            return side;
        }

        int Function::getAction(const MMAI::Schema::IState * s) {
            return f_getAction(s);
        }

        double Function::getValue(const MMAI::Schema::IState * s) {
            return f_getValue(s);
        }
    }
}
