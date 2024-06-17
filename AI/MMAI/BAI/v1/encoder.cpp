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

#include "schema/v1/types.h"
#include "schema/v1/constants.h"
#include "BAI/v1/encoder.h"

namespace MMAI::BAI::V1 {
    using namespace Schema::V1;
    using BS = Schema::BattlefieldState;

    // Example: v=1, vmax=5
    //  => add 0.2
    void Encoder::EncodeFloating(const int &v, const int &vmax, BS &vec) {
        // XXX: this is a simplified version for 0..1 norm
        vec.push_back(static_cast<float>(v) / static_cast<float>(vmax));
    }

    // Example: v=5, n=4
    //  Represent 5 as a 4-bit binary (LSB first)
    //  => add [1,0,1,0]
    void Encoder::EncodeBinary(const int &v, const int &n, const int &vmax, BS &vec) {
        int vtmp = v;
        for (int i=0; i < n; i++) {
            vec.push_back(vtmp % 2);
            vtmp /= 2;
        }
    }

    // Example: v=2, n=3
    //  Add v=2 ones and 3-2=1 zero
    void Encoder::EncodeNumeric(const int &v, const int &n, const int &vmax, BS &vec) {
        int n_ones = v;
        vec.insert(vec.end(), n_ones, 1);
        vec.insert(vec.end(), n - n_ones, 0);
    }

    // Example: v=10, n=4
    //  Add int(sqrt(10))=3 ones and 4-3=1 zero
    //  => add [1,1,1,0]
    void Encoder::EncodeNumericSqrt(const int &v, const int &n, const int &vmax, BS &vec) {
        int n_ones = int(std::sqrt(v));
        vec.insert(vec.end(), n_ones, 1);
        vec.insert(vec.end(), n - n_ones, 0);
    }

    // Example: v=1, n=5
    //  => add [0,1,0,0,0]
    void Encoder::EncodeCategorical(const int &v, const int &n, const int &vmax, BS &vec) {
        for (int i=0; i < n; i++)
            vec.push_back(i == v);
    }

    void Encoder::Encode(const HexAttribute &a, const int &v, BS &vec) {
        auto &[_, e, n, vmax] = HEX_ENCODING.at(EI(a));

        if (v == BATTLEFIELD_STATE_VALUE_NA) {
            if (e == Encoding::FLOATING)
                vec.push_back(BATTLEFIELD_STATE_VALUE_NA);
            else
                vec.insert(vec.end(), n, BATTLEFIELD_STATE_VALUE_NA);

            return;
        }

        if (v > vmax)
            THROW_FORMAT("Cannot encode value: %d (vmax=%d, a=%d, n=%d)", v % vmax % EI(a) % n);

        switch (e) {
        break; case Encoding::FLOATING: EncodeFloating(v, vmax, vec);
        break; case Encoding::BINARY: EncodeBinary(v, n, vmax, vec);
        break; case Encoding::NUMERIC: EncodeNumeric(v, n, vmax, vec);
        break; case Encoding::NUMERIC_SQRT: EncodeNumericSqrt(v, n, vmax, vec);
        break; case Encoding::CATEGORICAL: EncodeCategorical(v, n, vmax, vec);
        break; default:
            THROW_FORMAT("Unexpected Encoding: %d", EI(e));
        }
    }
}
