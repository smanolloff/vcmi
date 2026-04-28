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

#include "AI/MMAI/common.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace Detail
{
namespace S15 = Schema::V15;
using Encoding = S15::Encoding;
using EncodedVector = Schema::BattlefieldState;

inline void addZerosAndReturn(int n, EncodedVector & out)
{
	out.insert(out.end(), n, 0);
}

inline void encodeAccumulating(int v, int n, EncodedVector & out)
{
	out.insert(out.end(), v + 1, 1);
	out.insert(out.end(), n - v - 1, 0);
}

inline void encodeBinary(int v, int n, EncodedVector & out)
{
	if(v <= 0)
	{
		addZerosAndReturn(n, out);
		return;
	}

	int value = v;
	for(int i = 0; i < n; ++i)
	{
		out.push_back(value % 2);
		value /= 2;
	}
}

inline void encodeCategorical(int v, int n, EncodedVector & out)
{
	if(v <= 0)
	{
		out.push_back(1);
		addZerosAndReturn(n - 1, out);
		return;
	}

	for(int i = 0; i < n; ++i)
	{
		if(i == v)
		{
			out.push_back(1);
			addZerosAndReturn(n - i - 1, out);
			return;
		}

		out.push_back(0);
	}
}

inline float calcExpnorm(int v, int vmax, double slope)
{
	auto ratio = static_cast<double>(v) / vmax;
	return std::log1p(ratio * (std::exp(slope) - 1.0)) / (slope + 1e-6);
}

inline float calcLinnorm(int v, int vmax)
{
	return static_cast<float>(v) / static_cast<float>(vmax);
}

inline void encodeExpnorm(int v, int vmax, double slope, EncodedVector & out)
{
	if(v <= 0)
	{
		out.push_back(0);
		return;
	}

	out.push_back(calcExpnorm(v, vmax, slope));
}

inline void encodeLinnorm(int v, int vmax, EncodedVector & out)
{
	if(v <= 0)
	{
		out.push_back(0);
		return;
	}

	out.push_back(calcLinnorm(v, vmax));
}

inline void encodeValue(Encoding e, int n, int vmax, double p, int v, EncodedVector & out)
{
	auto value = std::min(v, vmax);

	switch(e)
	{
		case Encoding::ACCUMULATING_EXPLICIT_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.push_back(1);
				addZerosAndReturn(n - 1, out);
				return;
			}
			out.push_back(0);
			encodeAccumulating(value, n - 1, out);
			return;
		case Encoding::ACCUMULATING_IMPLICIT_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				addZerosAndReturn(n, out);
				return;
			}
			encodeAccumulating(value, n, out);
			return;
		case Encoding::ACCUMULATING_MASKING_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.insert(out.end(), n, S15::NULL_VALUE_ENCODED);
				return;
			}
			encodeAccumulating(value, n, out);
			return;
		case Encoding::ACCUMULATING_STRICT_NULL:
			ASSERT(value != S15::NULL_VALUE_UNENCODED, "NULL values are not allowed for strict encoding");
			encodeAccumulating(value, n, out);
			return;
		case Encoding::ACCUMULATING_ZERO_NULL:
			if(value <= 0)
			{
				out.push_back(1);
				addZerosAndReturn(n - 1, out);
				return;
			}
			encodeAccumulating(value, n, out);
			return;

		case Encoding::BINARY_EXPLICIT_NULL:
			out.push_back(value == S15::NULL_VALUE_UNENCODED);
			encodeBinary(value, n - 1, out);
			return;
		case Encoding::BINARY_MASKING_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.insert(out.end(), n, S15::NULL_VALUE_ENCODED);
				return;
			}
			encodeBinary(value, n, out);
			return;
		case Encoding::BINARY_STRICT_NULL:
			ASSERT(value != S15::NULL_VALUE_UNENCODED, "NULL values are not allowed for strict encoding");
			encodeBinary(value, n, out);
			return;
		case Encoding::BINARY_ZERO_NULL:
			encodeBinary(value, n, out);
			return;

		case Encoding::CATEGORICAL_EXPLICIT_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.push_back(1);
				addZerosAndReturn(n - 1, out);
				return;
			}
			out.push_back(0);
			encodeCategorical(value, n - 1, out);
			return;
		case Encoding::CATEGORICAL_IMPLICIT_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				addZerosAndReturn(n, out);
				return;
			}
			encodeCategorical(value, n, out);
			return;
		case Encoding::CATEGORICAL_MASKING_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.insert(out.end(), n, S15::NULL_VALUE_ENCODED);
				return;
			}
			encodeCategorical(value, n, out);
			return;
		case Encoding::CATEGORICAL_STRICT_NULL:
			ASSERT(value != S15::NULL_VALUE_UNENCODED, "NULL values are not allowed for strict encoding");
			encodeCategorical(value, n, out);
			return;
		case Encoding::CATEGORICAL_ZERO_NULL:
			encodeCategorical(value, n, out);
			return;

		case Encoding::EXPNORM_EXPLICIT_NULL:
			out.push_back(value == S15::NULL_VALUE_UNENCODED);
			encodeExpnorm(value, vmax, p, out);
			return;
		case Encoding::EXPNORM_MASKING_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.push_back(S15::NULL_VALUE_ENCODED);
				return;
			}
			encodeExpnorm(value, vmax, p, out);
			return;
		case Encoding::EXPNORM_STRICT_NULL:
			ASSERT(value != S15::NULL_VALUE_UNENCODED, "NULL values are not allowed for strict encoding");
			encodeExpnorm(value, vmax, p, out);
			return;
		case Encoding::EXPNORM_ZERO_NULL:
			encodeExpnorm(value, vmax, p, out);
			return;

		case Encoding::LINNORM_EXPLICIT_NULL:
			out.push_back(value == S15::NULL_VALUE_UNENCODED);
			encodeLinnorm(value, vmax, out);
			return;
		case Encoding::LINNORM_MASKING_NULL:
			if(value == S15::NULL_VALUE_UNENCODED)
			{
				out.push_back(S15::NULL_VALUE_ENCODED);
				return;
			}
			encodeLinnorm(value, vmax, out);
			return;
		case Encoding::LINNORM_STRICT_NULL:
			ASSERT(value != S15::NULL_VALUE_UNENCODED, "NULL values are not allowed for strict encoding");
			encodeLinnorm(value, vmax, out);
			return;
		case Encoding::LINNORM_ZERO_NULL:
			encodeLinnorm(value, vmax, out);
			return;

		case Encoding::RAW:
			out.push_back(value);
			return;
		default:
			THROW_FORMAT("Unexpected Encoding: %d", EI(e));
	}
}
}

template <size_t N, typename EncodingArray>
std::vector<float> encodeNodeAttributes(const std::array<int, N> & attrs, const EncodingArray & encoding)
{
	auto out = std::vector<float>{};
	out.reserve(Schema::V15::EncodedSize(encoding));

	for(size_t i = 0; i < N; ++i)
	{
		const auto & [_, e, n, vmax, p] = encoding.at(i);
		Detail::encodeValue(e, n, vmax, p, attrs.at(i), out);
	}

	return out;
}
}
