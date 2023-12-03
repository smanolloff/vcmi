#pragma once

#include "StdInc.h"

namespace MMAI {
    // Enum-to-int need C++23 to use std::to_underlying
    // https://en.cppreference.com/w/cpp/utility/to_underlying
    #define EI(enum_value) static_cast<int>(enum_value)
    #define ASSERT(cond, msg) if(!(cond)) throw std::runtime_error(std::string("AAI Assertion failed in ") + std::filesystem::path(__FILE__).filename().string() + ": " + msg)

    #define BF_XMAX 15    // GameConstants::BFIELD_WIDTH - 2 (ignore "side" cols)
    #define BF_YMAX 11    // GameConstants::BFIELD_HEIGHT
    #define BF_SIZE 165   // BF_XMAX * BF_YMAX
}
