/*
 * common.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "StdInc.h"
#include <sstream>

namespace MMAI {
    // Enum-to-int need C++23 to use std::to_underlying
    // https://en.cppreference.com/w/cpp/utility/to_underlying
    #define EI(enum_value) static_cast<int>(enum_value)
    #define ASSERT(cond, msg) if(!(cond)) throw std::runtime_error(std::string("Assertion failed in ") + boost::filesystem::path(__FILE__).filename().string() + ": " + msg)
    #define THROW_FORMAT(message, formatting_elems) throw std::runtime_error(boost::str(boost::format(message) % formatting_elems))

    #define BF_XMAX 15    // GameConstants::BFIELD_WIDTH - 2 (ignore "side" cols)
    #define BF_YMAX 11    // GameConstants::BFIELD_HEIGHT
    #define BF_SIZE 165   // BF_XMAX * BF_YMAX

    template <typename... Args>
    inline void expect(bool exp, const char* format, Args&&... args) {
        if (exp)
            return;

        constexpr std::size_t bufferSize = 2048;
        char buffer[bufferSize];

        std::snprintf(buffer, bufferSize, format, std::forward<Args>(args)...);
        throw std::runtime_error(buffer);
    }

    inline void expect(bool exp, const char* message) {
        if (exp) {
            return;
        }
        // No formatting; just throw with the message
        throw std::runtime_error(message);
    }
}
