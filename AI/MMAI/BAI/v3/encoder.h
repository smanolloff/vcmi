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

#include "schema/base.h"
#include "schema/v3/types.h"

namespace MMAI::BAI::V3 {
    using HexAttribute = Schema::V3::HexAttribute;
    using BS = Schema::BattlefieldState;

    class Encoder {
    public:
        static void Encode(const HexAttribute &a, const int &v, BS &vec);
        static void EncodeFloating(const int &v, const int &vmax, BS &vec);
        static void EncodeBinary(const int &v, const int &n, const int &vmax, BS &vec);
        static void EncodeNumeric(const int &v, const int &n, const int &vmax, BS &vec);
        static void EncodeNumericSqrt(const int &v, const int &n, const int &vmax, BS &vec);
        static void EncodeCategorical(const int &v, const int &n, const int &vmax, BS &vec);
    };
}
