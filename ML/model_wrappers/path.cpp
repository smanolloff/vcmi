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

#include "path.h"
#include "ML/MLClient.h"
#include <stdexcept>

namespace ML {
    namespace ModelWrappers {
        Path::Path(std::string path)
        : path(path) {};

        std::string Path::getName() {
            return path;
        };

        MMAI::Schema::ModelType Path::getType() {
            return MMAI::Schema::ModelType::PATH;
        };

        // The below methods should never be called on this object:
        // PATH models are dummy models which should not be used for anything
        // other than their getType() and getName() methods. Based on the return
        // value, a real Torch model should be loaded and used for the
        // upcoming battle instead.

        int Path::getVersion() {
            warn("getVersion", -666);
            return -666;
        };

        int Path::getAction(const MMAI::Schema::IState * s) {
            warn("getAction", -666);
            return -666;
        };

        double Path::getValue(const MMAI::Schema::IState * s) {
            warn("getValue", -666);
            return -666;
        };

        MMAI::Schema::Side Path::getSide() {
            warn("getSide", -666);
            return MMAI::Schema::Side(666);
        }

        void Path::warn(std::string m, int retval) {
            printf("WARNING: method %s called on a ModelWrapper object; returning %d\n", m.c_str(), retval);
        }
    }
}
