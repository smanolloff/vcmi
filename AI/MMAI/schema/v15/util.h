/*
 * util.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

namespace MMAI::Schema::V15
{
/*
 * Compile time int(sqrt(x))
 * https://stackoverflow.com/a/27709195
 */
template<typename T>
constexpr T Sqrt(T x, T lo, T hi)
{
	if(lo == hi)
		return lo;
	const T mid = (lo + hi + 1) / 2;
	return (x / mid < mid) ? Sqrt<T>(x, lo, mid - 1) : Sqrt(x, mid, hi);
}
template<typename T>
constexpr T CTSqrt(T x)
{
	return Sqrt<T>(x, 0, (x / 2) + 1);
}

/*
 * Compile time int(log(x, 2))
 * https://stackoverflow.com/a/23784921
 */
constexpr unsigned Log2(unsigned n)
{
	return n <= 1 ? 0 : 1 + Log2((n + 1) / 2);
}

/*
 * Compile-time checks for misconfigured `HEX_ENCODING`/`STACK_ENCODING`.
 * The index of the uninitialized element is returned.
 */
template<typename T>
constexpr int UninitializedEncodingAttributes(T elems)
{
	// E5S / E5H:
	using E5Type = typename T::value_type;

	// Stack Attribute / HexAttribute:
	using EnumType = std::tuple_element_t<0, E5Type>;

	for(int i = 0; i < EI(EnumType::_count); i++)
	{
		if(elems.at(i) == E5Type{})
			return EI(EnumType::_count) - i;
	}

	return 0;
}

/*
 * Compile-time checks for elements in `HEX_ENCODING` and `STACK_ENCODING`
 * which are out-of-order compared to the `Attribute` enum values.
 * The index at which the order is violated is returned.
 */
template<typename T>
constexpr int DisarrayedEncodingAttributeIndex(T elems)
{
	// E5S / E5H:
	using E5Type = typename T::value_type;

	// Stack Attribute / HexAttribute:
	using EnumType = std::tuple_element_t<0, E5Type>;

	for(int i = 0; i < EI(EnumType::_count); i++)
	{
		if(std::get<0>(elems.at(i)) != static_cast<EnumType>(i))
			return i;
	}

	return -1;
}

/*
 * Compile-time calculation for the encoded size of hexes and stacks
 */
template<typename T>
constexpr int EncodedSize(T elems)
{
	using E5Type = typename T::value_type;
	using EnumType = std::tuple_element_t<0, E5Type>;
	int ret = 0;
	for(int i = 0; i < EI(EnumType::_count); i++)
	{
		ret += std::get<2>(elems.at(i));
	}
	return ret;
}

}
