/*
 * encoder.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "schema/v13/constants.h"
#include "schema/v13/types.h"

#include "BAI/v13/encoder.h"
#include "common.h"

namespace MMAI::BAI::V13
{

using Schema::V13::Encoding;
using Schema::V13::GLOBAL_ENCODING;
using Schema::V13::HEX_ENCODING;
using Schema::V13::NULL_VALUE_ENCODED;
using Schema::V13::NULL_VALUE_UNENCODED;
using Schema::V13::PLAYER_ENCODING;

using BS = Schema::BattlefieldState;
using clock = std::chrono::system_clock;

#define ADD_ZEROS_AND_RETURN(n, vec) \
	vec.insert(vec.end(), n, 0);     \
	return;

#define MAYBE_ADD_ZEROS_AND_RETURN(v, n, vec) \
	if(v <= 0)                                \
	{                                         \
		ADD_ZEROS_AND_RETURN(n, vec)          \
	}

#define MAYBE_ADD_MASKED_AND_RETURN(v, n, vec)        \
	if(v == NULL_VALUE_UNENCODED)                     \
	{                                                 \
		vec.insert(vec.end(), n, NULL_VALUE_ENCODED); \
		return;                                       \
	}

#define MAYBE_THROW_STRICT_ERROR(v)                                                  \
	if(v == NULL_VALUE_UNENCODED)                                                    \
	{                                                                                \
		throw std::runtime_error("NULL values are not allowed for strict encoding"); \
	}

void Encoder::Encode(const std::string_view & attrname, int a, Encoding e, int n, int vmax, double p, int v, BS & vec)
{
	if(e == Encoding::RAW)
	{
		vec.push_back(v);
		return;
	}

	if(v > vmax)
	{
		// THROW_FORMAT("Cannot encode value: %d (vmax=%d, a=%d, n=%d, e=%d)", v % vmax % EI(a) % n % EI(e));
		// Can happen (e.g. DMG_*_ACC_REL0 > 1 if there were resurrected stacks)

		// Warn at most once every 600s
		auto now = clock::now();
		static thread_local std::map<std::string, std::map<int, clock::time_point>> warns;
		auto & warned_at = warns[std::string(attrname)][EI(a)];

		if(std::chrono::duration_cast<std::chrono::seconds>(now - warned_at) > std::chrono::seconds(600))
		{
			// This is not critical; the value will be capped to vmax (should not occur often)
			logAi->info("MMAI: Attribute value out of bounds: v=%d (vmax=%d, a=%d, e=%d, n=%d, attrname=%s)\n", v, vmax, EI(a), EI(e), n, attrname);
			warns[std::string(attrname)][EI(a)] = now;
		}
		v = vmax;
	}

	switch(e)
	{
		case Encoding::BINARY_EXPLICIT_NULL:
			EncodeBinaryExplicitNull(v, n, vec);
			break;
		case Encoding::BINARY_MASKING_NULL:
			EncodeBinaryMaskingNull(v, n, vec);
			break;
		case Encoding::BINARY_STRICT_NULL:
			EncodeBinaryStrictNull(v, n, vec);
			break;
		case Encoding::BINARY_ZERO_NULL:
			EncodeBinaryZeroNull(v, n, vec);
			break;
		case Encoding::EXPNORM_EXPLICIT_NULL:
			EncodeExpnormExplicitNull(v, vmax, p, vec);
			break;
		case Encoding::EXPNORM_MASKING_NULL:
			EncodeExpnormMaskingNull(v, vmax, p, vec);
			break;
		case Encoding::EXPNORM_STRICT_NULL:
			EncodeExpnormStrictNull(v, vmax, p, vec);
			break;
		case Encoding::EXPNORM_ZERO_NULL:
			EncodeExpnormZeroNull(v, vmax, p, vec);
			break;
		case Encoding::LINNORM_EXPLICIT_NULL:
			EncodeLinnormExplicitNull(v, vmax, vec);
			break;
		case Encoding::LINNORM_MASKING_NULL:
			EncodeLinnormMaskingNull(v, vmax, vec);
			break;
		case Encoding::LINNORM_STRICT_NULL:
			EncodeLinnormStrictNull(v, vmax, vec);
			break;
		case Encoding::LINNORM_ZERO_NULL:
			EncodeLinnormZeroNull(v, vmax, vec);
			break;
		case Encoding::CATEGORICAL_EXPLICIT_NULL:
			EncodeCategoricalExplicitNull(v, n, vec);
			break;
		case Encoding::CATEGORICAL_IMPLICIT_NULL:
			EncodeCategoricalImplicitNull(v, n, vec);
			break;
		case Encoding::CATEGORICAL_MASKING_NULL:
			EncodeCategoricalMaskingNull(v, n, vec);
			break;
		case Encoding::CATEGORICAL_STRICT_NULL:
			EncodeCategoricalStrictNull(v, n, vec);
			break;
		case Encoding::CATEGORICAL_ZERO_NULL:
			EncodeCategoricalZeroNull(v, n, vec);
			break;
		case Encoding::ACCUMULATING_EXPLICIT_NULL:
			EncodeAccumulatingExplicitNull(v, n, vec);
			break;
		case Encoding::ACCUMULATING_IMPLICIT_NULL:
			EncodeAccumulatingImplicitNull(v, n, vec);
			break;
		case Encoding::ACCUMULATING_MASKING_NULL:
			EncodeAccumulatingMaskingNull(v, n, vec);
			break;
		case Encoding::ACCUMULATING_STRICT_NULL:
			EncodeAccumulatingStrictNull(v, n, vec);
			break;
		case Encoding::ACCUMULATING_ZERO_NULL:
			EncodeAccumulatingZeroNull(v, n, vec);
			break;
		default:
			THROW_FORMAT("Unexpected Encoding: %d", EI(e));
	}
}

void Encoder::Encode(const HexAttribute a, int v, BS & vec)
{
	const auto & [_, e, n, vmax, p] = HEX_ENCODING.at(EI(a));
	Encode("HexAttribute", EI(a), e, n, vmax, p, v, vec);
}

void Encoder::Encode(const PlayerAttribute a, int v, BS & vec)
{
	const auto & [_, e, n, vmax, p] = PLAYER_ENCODING.at(EI(a));
	Encode("PlayerAttribute", EI(a), e, n, vmax, p, v, vec);
}

void Encoder::Encode(const GlobalAttribute a, int v, BS & vec)
{
	const auto & [_, e, n, vmax, p] = GLOBAL_ENCODING.at(EI(a));
	Encode("GlobalAttribute", EI(a), e, n, vmax, p, v, vec);
}

//
// ACCUMULATING
//
void Encoder::EncodeAccumulatingExplicitNull(int v, int n, BS & vec)
{
	if(v == NULL_VALUE_UNENCODED)
	{
		vec.push_back(1);
		ADD_ZEROS_AND_RETURN(n - 1, vec);
	};
	vec.push_back(0);
	EncodeAccumulating(v, n - 1, vec);
}

void Encoder::EncodeAccumulatingImplicitNull(int v, int n, BS & vec)
{
	if(v == NULL_VALUE_UNENCODED)
	{
		ADD_ZEROS_AND_RETURN(n, vec);
	}
	EncodeAccumulating(v, n, vec);
}

void Encoder::EncodeAccumulatingMaskingNull(int v, int n, BS & vec)
{
	MAYBE_ADD_MASKED_AND_RETURN(v, n, vec);
	EncodeAccumulating(v, n, vec);
}

void Encoder::EncodeAccumulatingStrictNull(int v, int n, BS & vec)
{
	MAYBE_THROW_STRICT_ERROR(v);
	EncodeAccumulating(v, n, vec);
}

void Encoder::EncodeAccumulatingZeroNull(int v, int n, BS & vec)
{
	if(v <= 0)
	{
		vec.push_back(1);
		ADD_ZEROS_AND_RETURN(n - 1, vec);
	}
	EncodeAccumulating(v, n, vec);
}

void Encoder::EncodeAccumulating(int v, int n, BS & vec)
{
	vec.insert(vec.end(), v + 1, 1);
	vec.insert(vec.end(), n - v - 1, 0);
}

//
// BINARY
//

void Encoder::EncodeBinaryExplicitNull(int v, int n, BS & vec)
{
	vec.push_back(v == NULL_VALUE_UNENCODED);
	EncodeBinary(v, n - 1, vec);
}

void Encoder::EncodeBinaryMaskingNull(int v, int n, BS & vec)
{
	MAYBE_ADD_MASKED_AND_RETURN(v, n, vec);
	EncodeBinary(v, n, vec);
}

void Encoder::EncodeBinaryStrictNull(int v, int n, BS & vec)
{
	MAYBE_THROW_STRICT_ERROR(v);
	EncodeBinary(v, n, vec);
}

void Encoder::EncodeBinaryZeroNull(int v, int n, BS & vec)
{
	EncodeBinary(v, n, vec);
}

void Encoder::EncodeBinary(int v, int n, BS & vec)
{
	MAYBE_ADD_ZEROS_AND_RETURN(v, n, vec);

	int vtmp = v;
	for(int i = 0; i < n; ++i)
	{
		vec.push_back(vtmp % 2);
		vtmp /= 2;
	}
}

//
// CATEGORICAL
//

void Encoder::EncodeCategoricalExplicitNull(int v, int n, BS & vec)
{
	if(v == NULL_VALUE_UNENCODED)
	{
		vec.push_back(v == NULL_VALUE_UNENCODED);
		ADD_ZEROS_AND_RETURN(n - 1, vec);
	}
	vec.push_back(0);
	EncodeCategorical(v, n - 1, vec);
}

void Encoder::EncodeCategoricalImplicitNull(int v, int n, BS & vec)
{
	if(v == NULL_VALUE_UNENCODED)
	{
		ADD_ZEROS_AND_RETURN(n, vec);
	}

	EncodeCategorical(v, n, vec);
}

void Encoder::EncodeCategoricalMaskingNull(int v, int n, BS & vec)
{
	MAYBE_ADD_MASKED_AND_RETURN(v, n, vec);
	EncodeCategorical(v, n, vec);
}

void Encoder::EncodeCategoricalStrictNull(int v, int n, BS & vec)
{
	MAYBE_THROW_STRICT_ERROR(v);
	EncodeCategorical(v, n, vec);
}

void Encoder::EncodeCategoricalZeroNull(int v, int n, BS & vec)
{
	EncodeCategorical(v, n, vec);
}

void Encoder::EncodeCategorical(int v, int n, BS & vec)
{
	if(v <= 0)
	{
		vec.push_back(1);
		ADD_ZEROS_AND_RETURN(n - 1, vec);
	}

	for(int i = 0; i < n; ++i)
	{
		if(i == v)
		{
			vec.push_back(1);
			ADD_ZEROS_AND_RETURN(n - i - 1, vec);
		}
		else
		{
			vec.push_back(0);
		}
	}
}

//
// EXPNORM
//

void Encoder::EncodeExpnormExplicitNull(int v, int vmax, double slope, BS & vec)
{
	vec.push_back(v == NULL_VALUE_UNENCODED);
	EncodeExpnorm(v, vmax, slope, vec);
}

void Encoder::EncodeExpnormMaskingNull(int v, int vmax, double slope, BS & vec)
{
	if(v == NULL_VALUE_UNENCODED)
	{
		vec.push_back(NULL_VALUE_ENCODED);
		return;
	}
	EncodeExpnorm(v, vmax, slope, vec);
}

void Encoder::EncodeExpnormStrictNull(int v, int vmax, double slope, BS & vec)
{
	MAYBE_THROW_STRICT_ERROR(v);
	EncodeExpnorm(v, vmax, slope, vec);
}

void Encoder::EncodeExpnormZeroNull(int v, int vmax, double slope, BS & vec)
{
	EncodeExpnorm(v, vmax, slope, vec);
}

void Encoder::EncodeExpnorm(int v, int vmax, double slope, BS & vec)
{
	if(v <= 0)
	{
		vec.push_back(0);
		return;
	}

	vec.push_back(CalcExpnorm(v, vmax, slope));
}

// Visualise on https://www.desmos.com/calculator:
// ln(1 + (x/M) * (exp(S)-1))/S
// Add slider "S" (slope) and "M" (vmax).
// Play with the sliders to see the nonlinearity (use M=1 for best view)
// XXX: slope cannot be 0
float Encoder::CalcExpnorm(int v, int vmax, double slope)
{
	auto ratio = static_cast<double>(v) / vmax;
	return std::log1p(ratio * (std::exp(slope) - 1.0)) / (slope + 1e-6);
}

//
// LINNORM
//

void Encoder::EncodeLinnormExplicitNull(int v, int vmax, BS & vec)
{
	vec.push_back(v == NULL_VALUE_UNENCODED);
	EncodeLinnorm(v, vmax, vec);
}

void Encoder::EncodeLinnormMaskingNull(int v, int vmax, BS & vec)
{
	if(v == NULL_VALUE_UNENCODED)
	{
		vec.push_back(NULL_VALUE_ENCODED);
		return;
	}
	EncodeLinnorm(v, vmax, vec);
}

void Encoder::EncodeLinnormStrictNull(int v, int vmax, BS & vec)
{
	MAYBE_THROW_STRICT_ERROR(v);
	EncodeLinnorm(v, vmax, vec);
}

void Encoder::EncodeLinnormZeroNull(int v, int vmax, BS & vec)
{
	EncodeLinnorm(v, vmax, vec);
}

void Encoder::EncodeLinnorm(int v, int vmax, BS & vec)
{
	if(v <= 0)
	{
		vec.push_back(0);
		return;
	}

	// XXX: this is a simplified version for 0..1 norm
	vec.push_back(CalcLinnorm(v, vmax));
}

float Encoder::CalcLinnorm(int v, int vmax)
{
	return static_cast<float>(v) / static_cast<float>(vmax);
}
}
