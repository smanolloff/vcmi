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
#include "schema/v13/types.h"

namespace MMAI::BAI::V13
{
using Schema::V13::GlobalAttribute;
using Schema::V13::HexAttribute;
using Schema::V13::PlayerAttribute;
using BS = Schema::BattlefieldState;

class Encoder
{
public:
	static void Encode(HexAttribute a, int v, BS & vec);
	static void Encode(PlayerAttribute a, int v, BS & vec);
	static void Encode(GlobalAttribute a, int v, BS & vec);
	static void Encode(const std::string_view & attrname, int a, Schema::V13::Encoding e, int n, int vmax, double p, int v, BS & vec);

	static void EncodeAccumulatingExplicitNull(int v, int n, BS & vec);
	static void EncodeAccumulatingImplicitNull(int v, int n, BS & vec);
	static void EncodeAccumulatingMaskingNull(int v, int n, BS & vec);
	static void EncodeAccumulatingStrictNull(int v, int n, BS & vec);
	static void EncodeAccumulatingZeroNull(int v, int n, BS & vec);

	static void EncodeBinaryExplicitNull(int v, int n, BS & vec);
	static void EncodeBinaryMaskingNull(int v, int n, BS & vec);
	static void EncodeBinaryStrictNull(int v, int n, BS & vec);
	static void EncodeBinaryZeroNull(int v, int n, BS & vec);

	static void EncodeCategoricalExplicitNull(int v, int n, BS & vec);
	static void EncodeCategoricalImplicitNull(int v, int n, BS & vec);
	static void EncodeCategoricalMaskingNull(int v, int n, BS & vec);
	static void EncodeCategoricalStrictNull(int v, int n, BS & vec);
	static void EncodeCategoricalZeroNull(int v, int n, BS & vec);

	static void EncodeExpnormExplicitNull(int v, int vmax, double slope, BS & vec);
	static void EncodeExpnormMaskingNull(int v, int vmax, double slope, BS & vec);
	static void EncodeExpnormStrictNull(int v, int vmax, double slope, BS & vec);
	static void EncodeExpnormZeroNull(int v, int vmax, double slope, BS & vec);

	static void EncodeLinnormExplicitNull(int v, int vmax, BS & vec);
	static void EncodeLinnormMaskingNull(int v, int vmax, BS & vec);
	static void EncodeLinnormStrictNull(int v, int vmax, BS & vec);
	static void EncodeLinnormZeroNull(int v, int vmax, BS & vec);

	static float CalcExpnorm(int v, int vmax, double slope);
	static float CalcLinnorm(int v, int vmax);

private:
	static void EncodeAccumulating(int v, int n, BS & vec);
	static void EncodeBinary(int v, int n, BS & vec);
	static void EncodeCategorical(int v, int n, BS & vec);
	static void EncodeExpnorm(int v, int vmax, double slope, BS & vec);
	static void EncodeLinnorm(int v, int vmax, BS & vec);
};
}
