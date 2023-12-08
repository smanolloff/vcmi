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
