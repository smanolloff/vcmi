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

namespace MMAI::BAI::V13 {
    using GlobalAttribute = Schema::V13::GlobalAttribute;
    using PlayerAttribute = Schema::V13::PlayerAttribute;
    using HexAttribute = Schema::V13::HexAttribute;
    using BS = Schema::BattlefieldState;

    class Encoder {
    public:
        static void Encode(const HexAttribute a, const int v, BS &vec);
        static void Encode(const PlayerAttribute a, const int v, BS &vec);
        static void Encode(const GlobalAttribute a, const int v, BS &vec);
        static void Encode(
            const char* attrtype,
            const int a,
            const Schema::V13::Encoding e,
            const int n,
            const int vmax,
            const double p,
            int v,
            BS &vec
        );

        static void EncodeAccumulatingExplicitNull(const int v, const int n, BS &vec);
        static void EncodeAccumulatingImplicitNull(const int v, const int n, BS &vec);
        static void EncodeAccumulatingMaskingNull(const int v, const int n, BS &vec);
        static void EncodeAccumulatingStrictNull(const int v, const int n, BS &vec);
        static void EncodeAccumulatingZeroNull(const int v, const int n, BS &vec);

        static void EncodeBinaryExplicitNull(const int v, const int n, BS &vec);
        static void EncodeBinaryMaskingNull(const int v, const int n, BS &vec);
        static void EncodeBinaryStrictNull(const int v, const int n, BS &vec);
        static void EncodeBinaryZeroNull(const int v, const int n, BS &vec);

        static void EncodeCategoricalExplicitNull(const int v, const int n, BS &vec);
        static void EncodeCategoricalImplicitNull(const int v, const int n, BS &vec);
        static void EncodeCategoricalMaskingNull(const int v, const int n, BS &vec);
        static void EncodeCategoricalStrictNull(const int v, const int n, BS &vec);
        static void EncodeCategoricalZeroNull(const int v, const int n, BS &vec);

        static void EncodeExpnormExplicitNull(const int v, const int vmax, double slope, BS &vec);
        static void EncodeExpnormMaskingNull(const int v, const int vmax, double slope, BS &vec);
        static void EncodeExpnormStrictNull(const int v, const int vmax, double slope, BS &vec);
        static void EncodeExpnormZeroNull(const int v, const int vmax, double slope, BS &vec);

        static void EncodeLinnormExplicitNull(const int v, const int vmax, BS &vec);
        static void EncodeLinnormMaskingNull(const int v, const int vmax, BS &vec);
        static void EncodeLinnormStrictNull(const int v, const int vmax, BS &vec);
        static void EncodeLinnormZeroNull(const int v, const int vmax, BS &vec);

        static float CalcExpnorm(const int v, const int vmax, double slope);
        static float CalcLinnorm(const int v, const int vmax);
    private:
        static void EncodeAccumulating(const int v, const int n, BS &vec);
        static void EncodeBinary(const int v, const int n, BS &vec);
        static void EncodeCategorical(const int v, const int n, BS &vec);
        static void EncodeExpnorm(const int v, const int vmax, double slope, BS &vec);
        static void EncodeLinnorm(const int v, const int vmax, BS &vec);
    };
}
