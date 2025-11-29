/*
 * util.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

namespace MMAI::BAI::V13 {
    namespace Util {
        int Damp(int v, int max) {
            return std::round(max * std::tanh(static_cast<double>(v) / max));
        }
    }
}
