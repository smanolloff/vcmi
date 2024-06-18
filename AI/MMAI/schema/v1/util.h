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

namespace MMAI::Schema::V1 {
    /*
     * Compile time int(sqrt(x))
     * https://stackoverflow.com/a/27709195
     */
    template <typename T> constexpr T Sqrt(T x, T lo, T hi) {
        if (lo == hi) return lo;
        const T mid = (lo + hi + 1) / 2;
        return (x / mid < mid) ? Sqrt<T>(x, lo, mid - 1) : Sqrt(x, mid, hi);
    }
    template <typename T> constexpr T CTSqrt(T x) { return Sqrt<T>(x, 0, x / 2 + 1); }

    /*
     * Compile time int(log(x, 2))
     * https://stackoverflow.com/a/23784921
     */
    constexpr unsigned Log2(unsigned n) {
        return n <= 1 ? 0 : 1 + Log2((n + 1) / 2);
    }
}
