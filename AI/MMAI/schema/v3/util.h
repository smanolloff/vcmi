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

#include "./types.h"

namespace MMAI::Schema::V3 {
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

    /*
     * Compile-time checks for misconfigured `HEX_ENCODING`/`STACK_ENCODING`.
     * The index of the uninitialized element is returned.
     */
    template <typename T>
    constexpr int UninitializedEncodingAttributes(T elems) {
        // E4S / E4H:
        using E4Type = typename T::value_type;

        // Stack Attribute / HexAttribute:
        using EnumType = typename std::tuple_element<0, E4Type>::type;

        for (int i = 0; i < EI(EnumType::_count); i++) {
            if (elems.at(i) == E4Type{}) return EI(EnumType::_count) - i;
        }

        return 0;
    }

    /*
     * Compile-time checks for elements in `HEX_ENCODING` and `STACK_ENCODING`
     * which are out-of-order compared to the `Attribute` enum values.
     * The index at which the order is violated is returned.
     */
    template <typename T>
    constexpr int DisarrayedEncodingAttributeIndex(T elems) {
        // E4S / E4H:
        using E4Type = typename T::value_type;

        // Stack Attribute / HexAttribute:
        using EnumType = typename std::tuple_element<0, E4Type>::type;

        for (int i = 0; i < EI(EnumType::_count); i++) {
            if (std::get<0>(elems.at(i)) != EnumType(i)) return i;
        }

        return -1;
    }

    /*
     * Compile-time calculator for the number of unused values
     * in a (potentially sub-optimal) BINARY encoding definition.
     * Thue number of unuxed values is returned.
     *
     * Example:
     * `vmax=130` means that 8 bits will be needed for the necoding (`n=8`).
     * The maximum number of values which can be encoded with 8 bits is 255
     * so there are 255-131=125 unused values => `125` is returned.
     */
    constexpr int BinaryAttributeUnusedValues(Encoding e, int n, int vmax) {
        switch(e) {
        break; case Encoding::BINARY_EXPLICIT_NULL: return ((1<<(n-1)) - 1 - vmax);
        break; case Encoding::BINARY_MASKING_NULL:  return ((1<<n) - 1 - vmax);
        break; case Encoding::BINARY_STRICT_NULL:   return ((1<<n) - 1 - vmax);
        break; case Encoding::BINARY_ZERO_NULL:     return ((1<<n) - 1 - vmax);
        default:
            return 0;
        }
        return 0;
    }

    template <typename T>
    constexpr int MiscalculatedBinaryAttributeUnusedValues(T elems) {
        int i = MiscalculatedBinaryAttributeIndex(elems);
        if (i == -1) return 0;
        auto [_, e, n, vmax] = elems.at(i);
        return BinaryAttributeUnusedValues(e, n, vmax);
    }

    /*
     * Compile-time locator of sub-optimal BINARY encodings.
     * (see BinaryAttributeUnusedValues())
     * The index of the sub-optimal BINARY encoding is returned.
     */
    template <typename T>
    constexpr int MiscalculatedBinaryAttributeIndex(T elems) {
        using E4Type = typename T::value_type;
        using EnumType = typename std::tuple_element<0, E4Type>::type;

        for (int i = 0; i < EI(EnumType::_count); i++) {
            auto [_, e, n, vmax] = elems.at(i);
            if (BinaryAttributeUnusedValues(e, n, vmax) > 0) return i;
        }

        return -1;
    }

    /*
     * Compile-time calculation for the encoded size of hexes and stacks
     */
    template <typename T>
    constexpr int EncodedSize(T elems) {
        using E4Type = typename T::value_type;
        using EnumType = typename std::tuple_element<0, E4Type>::type;
        int ret = 0;
        for (int i = 0; i < EI(EnumType::_count); i++) {
            ret += std::get<2>(elems.at(i));
        }
        return ret;
    }
}
