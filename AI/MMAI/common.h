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

// used when including loader.h
#define __LOADED_FROM_MMAI 1

#include "StdInc.h"

namespace MMAI {
    // Enum-to-int need C++23 to use std::to_underlying
    // https://en.cppreference.com/w/cpp/utility/to_underlying
    #define EI(enum_value) static_cast<int>(enum_value)
    #define ASSERT(cond, msg) if(!(cond)) throw std::runtime_error(std::string("Assertion failed in ") + std::filesystem::path(__FILE__).filename().string() + ": " + msg)

    #define BF_XMAX 15    // GameConstants::BFIELD_WIDTH - 2 (ignore "side" cols)
    #define BF_YMAX 11    // GameConstants::BFIELD_HEIGHT
    #define BF_SIZE 165   // BF_XMAX * BF_YMAX

    inline void expect(bool exp, const char* format, ...) {
        if (exp) return;

        constexpr int bufferSize = 2048; // Adjust this size according to your needs
        char buffer[bufferSize];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, bufferSize, format, args);
        va_end(args);

        throw std::runtime_error(buffer);
    }

}
