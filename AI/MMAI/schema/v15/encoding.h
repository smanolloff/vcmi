/*
 * encoding.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include <cstdint>

namespace MMAI::Schema::V15
{
enum class Encoding : uint8_t
{
    /*
     * Represent `v` as `n` bits, where `bits[1..v+1]=1`.
     * If `v=-1` (a.k.a. "NULL"), only the bit at index 0 will be `1`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,1,1,1,1]`
     * * `v=0`,  `n=5` => `[0,1,0,0,0]`
     * * `v=-1`, `n=5` => `[1,0,0,0,0]`
     */
    ACCUMULATING_EXPLICIT_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[0..v]=1`.
     * If `v=-1` (a.k.a. "NULL"), all bits will be `0`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,1,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => `[0,0,0,0,0]`
     */
    ACCUMULATING_IMPLICIT_NULL,
    /*
     * Represent `v` as `n` bits, where `bits[0..v]=1`.
     * If `v=-1` (a.k.a. "NULL"), all bits will be `-1`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,1,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => `[-1,-1,-1,-1,-1]`
     */
    ACCUMULATING_MASKING_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[0..v]=1`.
     * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,1,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => (error)
     */
    ACCUMULATING_STRICT_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[0..v]=1`.
     * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,1,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => `[1,0,0,0,0]`
     */
    ACCUMULATING_ZERO_NULL,

    /*
     * Represent `v<<1` as `n`-bit binary (unsigned, LSB at index 0).
     * If `v=-1` (a.k.a. "NULL"), only the bit at index 0 will be `1`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,1,1,0,0]`
     * * `v=0`,  `n=5` => `[0,0,0,0,0]`
     * * `v=-1`, `n=5` => `[1,0,0,0,0]`
     */
    BINARY_EXPLICIT_NULL,

    /*
     * Represent `v` as an `n`-bit binary (LSB at index 0)
     * If `v=-1` (a.k.a. "NULL"), all bits will be `-1`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,0,0,0]`
     * * `v=0`,  `n=5` => `[0,0,0,0,0]`
     * * `v=-1`, `n=5` => `[-1,-1,-1,-1,-1]`
     */
    BINARY_MASKING_NULL,

    /*
     * Represent `v` as an `n`-bit binary (LSB at index 0)
     * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,0,0,0]`
     * * `v=0`,  `n=5` => `[0,0,0,0,0]`
     * * `v=-1`, `n=5` => (error)
     */
    BINARY_STRICT_NULL,

    /*
     * Represent `v` as `n`-bit binary (unsigned, LSB at index 0).
     * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[1,1,0,0,0]`
     * * `v=0`,  `n=5` => `[0,0,0,0,0]`
     * * `v=-1`, `n=5` => `[0,0,0,0,0]`
     */
    BINARY_ZERO_NULL,
    // XXX: BINARY_ZERO_NULL obsoletes BINARY_IMPLICIT_NULL

    /*
     * Represent `v` as `n` bits, where `bits[v+1]=1`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,0,0,0,1]`
     * * `v=0`,  `n=5` => `[0,1,0,0,0]`
     * * `v=-1`, `n=5` => `[1,0,0,0,0]`
     */
    CATEGORICAL_EXPLICIT_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[v]=1`.
     * If `v=-1` (a.k.a. "NULL"), all bits will be `0`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,0,0,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => `[0,0,0,0,0]`
     */
    CATEGORICAL_IMPLICIT_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[v]=1`.
     * If `v=-1` (a.k.a. "NULL"), all bits will be `-1`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,0,0,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => `[-1,-1,-1,-1,-1]`
     */
    CATEGORICAL_MASKING_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[v]=1`.
     * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,0,0,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => (error)
     */
    CATEGORICAL_STRICT_NULL,

    /*
     * Represent `v` as `n` bits, where `bits[v]=1`.
     * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
     *
     * Examples:
     * * `v=3`,  `n=5` => `[0,0,0,1,0]`
     * * `v=0`,  `n=5` => `[1,0,0,0,0]`
     * * `v=-1`, `n=5` => `[1,0,0,0,0]`
     */
    CATEGORICAL_ZERO_NULL,

    /*
     * Normalize `v+1` exponentially with base `vmax`
     * and represent it as `[0, vnorm]`.
     * If `v=-1` (a.k.a. "NULL"), the result will be `[1, 0]
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `[0, 0.6]`
     * * `v=0`,  `vmax=10` => `[0, 0]`
     * * `v=-1`, `vmax=10` => `[1, 0]`
     */
    EXPNORM_EXPLICIT_NULL,

    /*
     * Normalize `v+1` exponentially with base `vmax`.
     * If `v=0`, en error will be thrown.
     * If `v=-1` (a.k.a. "NULL"), it will not be normalized.
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `0.6`
     * * `v=0`,  `vmax=10` => `0`
     * * `v=-1`, `vmax=10` => `-1`
     */
    EXPNORM_MASKING_NULL,

    /*
     * Normalize `v+1` exponentially with base `vmax`.
     * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `0.6`
     * * `v=0`,  `vmax=10` => `0`
     * * `v=-1`, `vmax=10` => (error)
     */
    EXPNORM_STRICT_NULL,

    /*
     * Normalize `v+1` exponentially with base `vmax`.
     * If `v=0`, en error will be thrown.
     * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `0.6`
     * * `v=0`,  `vmax=10` => `0`
     * * `v=-1`, `vmax=10` => `0`
     */
    EXPNORM_ZERO_NULL,
    // XXX: NORMALIZED_ZERO_NULL obsoletes NORMALIZED_IMPLICIT_NULL

    /*
     * Normalize `v` linearly in the range `(0, vmax)`.
     * and represent it as `[0, vnorm]`.
     * If `v=-1` (a.k.a. "NULL"), the result will be `[1, 0]
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `[0, 0.3]`
     * * `v=0`,  `vmax=10` => `[0, 0]`
     * * `v=-1`, `vmax=10` => `[1, 0]`
     */
    LINNORM_EXPLICIT_NULL,

    /*
     * Normalize `v` linearly in the range `(0, vmax)`.
     * If `v=-1` (a.k.a. "NULL"), it will not be normalized.
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `0.3`
     * * `v=0`,  `vmax=10` => `0`
     * * `v=-1`, `vmax=10` => `-1`
     */
    LINNORM_MASKING_NULL,

    /*
     * Normalize `v` linearly in the range `(0, vmax)`.
     * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `0.3`
     * * `v=0`,  `vmax=10` => `0`
     * * `v=-1`, `vmax=10` => (error)
     */
    LINNORM_STRICT_NULL,

    /*
     * Normalize `v` linearly in the range `(0, vmax)`.
     * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
     *
     * Examples:
     * * `v=3`,  `vmax=10` => `0.6`
     * * `v=0`,  `vmax=10` => `0`
     * * `v=-1`, `vmax=10` => `0`
     */
    LINNORM_ZERO_NULL,
    // XXX: NORMALIZED_ZERO_NULL obsoletes NORMALIZED_IMPLICIT_NULL

    /*
     * Don't normalize, use as-is.
     */
    RAW,
};
}
