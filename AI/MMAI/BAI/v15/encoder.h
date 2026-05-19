/*
 * encoder.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "schema/base.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15::Encoder
{
	using BS = Schema::BattlefieldState;
	struct EncoderInput
	{
		const std::string_view & attrname;
		const int a;
		const Schema::V15::Encoding e;
		const int n;
		const int vmax;
		const double p;
		const int v;
	};

	void Encode(const EncoderInput & in, BS & out);

	template <typename EncTraits>
	std::vector<float> Encode(const std::array<int, EncTraits::attr_count> & attrs)
	{
		auto out = std::vector<float>{};
		out.reserve(EncodedSize(EncTraits::encoding));

		for(size_t i = 0; i < attrs.size(); ++i)
		{
			const auto & [_, e, n, vmax, p] = EncTraits::encoding.at(i);
			Encode(EncoderInput{
				.attrname = EncTraits::name,
				.a = static_cast<int>(i),
				.e = e,
				.n = n,
				.vmax = vmax,
				.p = p,
				.v = attrs.at(i)
			}, out);
		}

		return out;
	}
};
