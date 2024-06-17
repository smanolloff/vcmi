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

#include "BAI/v2/encoder.h"
#include "schema/v1/constants.h"
#include "schema/v2/constants.h"

namespace MMAI::BAI::V2 {
    // Same as V1, but uses HEX_ENCODING from V2
    void Encoder::Encode(const HexAttribute &a, const int &v, BS &vec) {
        auto &[_, e, n, vmax] = Schema::V2::HEX_ENCODING.at(EI(a));

        if (v == Schema::V1::BATTLEFIELD_STATE_VALUE_NA) {
            vec.push_back(Schema::V1::BATTLEFIELD_STATE_VALUE_NA);
            return;
        }

        if (v > vmax)
            throw std::runtime_error(
                "Cannot encode value: " + std::to_string(v) \
                + "(vmax=" + std::to_string(vmax) \
                + ", a=" + std::to_string(EI(a)) \
                + ", n=" + std::to_string(n) + ")"
            );

        if (e != Schema::V1::Encoding::FLOATING)
            throw std::runtime_error("V2 encodes all values as floats");

        EncodeFloating(v, vmax, vec);
    }
}
