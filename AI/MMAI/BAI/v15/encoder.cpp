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
#include "schema/v15/types.h"

#include "BAI/v15/encoder.h"
#include "common.h"

namespace MMAI::BAI::V15::Encoder
{

using Encoding = Schema::V15::Encoding;
using BS = Schema::BattlefieldState;
using clock = std::chrono::system_clock;

namespace
{
	void EncodeCategorical(int v, int n, BS & out)
	{
		if(v <= 0)
		{
			out.push_back(1);
			out.insert((out).end(), n - 1, 0);
		}

		for(int i = 0; i < n; ++i)
		{
			if(i == v)
			{
				out.push_back(1);
				out.insert((out).end(), n - i - 1, 0);
			}
			else
			{
				out.push_back(0);
			}
		}
	}

	float CalcLinnorm(int v, int vmax)
	{
		return static_cast<float>(v) / static_cast<float>(vmax);
	}

	void EncodeLinnorm(int v, int vmax, BS & out)
	{
		if(v <= 0)
		{
			out.push_back(0);
			return;
		}

		// XXX: this is a simplified version for 0..1 norm
		out.push_back(CalcLinnorm(v, vmax));
	}
}

void Encode(const EncoderInput & in, BS & out)
{
	auto v = in.v;

	if(in.v > in.vmax || in.v < -in.vmax)
	{
		// Warn at most once every 600s
		auto now = clock::now();
		static thread_local std::map<std::string, std::map<int, clock::time_point>> warns;
		auto & warned_at = warns[std::string(in.attrname)][EI(in.a)];

		if(std::chrono::duration_cast<std::chrono::seconds>(now - warned_at) > std::chrono::seconds(600))
		{
			// This is not critical; the value will be capped to vmax (should not occur often)
			logAi->info(
				"MMAI: Attribute value out of bounds: v=%d (vmax=|%d|, a=%d, e=%d, n=%d, attrname=%s)\n", in.v, in.vmax, EI(in.a), EI(in.e), in.n, in.attrname
			);
			warns[std::string(in.attrname)][EI(in.a)] = now;
		}
		v = v > 0 ? in.vmax : -in.vmax;
	}

	switch(in.e)
	{
		case Encoding::LINNORM:
			EncodeLinnorm(v, in.vmax, out);
			break;
		case Encoding::CATEGORICAL:
			EncodeCategorical(v, in.n, out);
			break;
		case Encoding::RAW:
			out.push_back(static_cast<float>(in.v));
			break;
		default:
			THROW_FORMAT("Unexpected Encoding: %d", EI(in.e));
	}
}

}
