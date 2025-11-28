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

namespace MMAI::BAI::V13 {
    using namespace Schema::V13;
    using BS = Schema::BattlefieldState;
    using clock = std::chrono::system_clock;

    static std::map<std::string, std::map<int, clock::time_point>> warns;

    #define COERCE(v, vfallback) (v == NULL_VALUE_UNENCODED) ? vfallback : v

    #define ADD_ZEROS_AND_RETURN(n, vec) \
            vec.insert(vec.end(), n, 0); \
            return;

    #define MAYBE_ADD_ZEROS_AND_RETURN(v, n, vec) \
        if (v <= 0) { ADD_ZEROS_AND_RETURN(n, vec) }

    #define MAYBE_ADD_MASKED_AND_RETURN(v, n, vec) \
        if (v == NULL_VALUE_UNENCODED) { \
            vec.insert(vec.end(), n, NULL_VALUE_ENCODED); \
            return; \
        }

    #define MAYBE_THROW_STRICT_ERROR(v) \
        if (v == NULL_VALUE_UNENCODED) { \
            throw std::runtime_error("NULL values are not allowed for strict encoding"); \
        }

    void Encoder::Encode(
        const char* attrname,
        const int a,
        const Encoding e,
        const int n,
        const int vmax,
        const double p,
        int v,
        BS &vec
    ) {
        if (e == Encoding::RAW) {
            vec.push_back(v);
            return;
        }

        if (v > vmax) {
            // THROW_FORMAT("Cannot encode value: %d (vmax=%d, a=%d, n=%d, e=%d)", v % vmax % EI(a) % n % EI(e));
            // Can happen (e.g. DMG_*_ACC_REL0 > 1 if there were resurrected stacks)

            // Warn at most once every 600s
            auto now = clock::now();
            auto warned_at = warns[attrname][EI(a)];

            // auto warned_at_ctime = clock::to_time_t(warned_at);
            // std::cout << "Warned at: " << std::ctime(&warned_at_ctime) << "\n";
            if (std::chrono::duration_cast<std::chrono::seconds>(now - warned_at) > std::chrono::seconds(600)) {
                // This is not critical; the value will be capped to vmax (should not occur often)
                logAi->debug("Attribute value out of bounds: v=%d (vmax=%d, a=%d, e=%d, n=%d, attrname=%s)\n", v, vmax, EI(a), EI(e), n, attrname);
                warns[attrname][EI(a)] = now;
            }
            v = vmax;
        }

        switch (e) {
        break; case Encoding::BINARY_EXPLICIT_NULL: EncodeBinaryExplicitNull(v, n, vec);
        break; case Encoding::BINARY_MASKING_NULL: EncodeBinaryMaskingNull(v, n, vec);
        break; case Encoding::BINARY_STRICT_NULL: EncodeBinaryStrictNull(v, n, vec);
        break; case Encoding::BINARY_ZERO_NULL: EncodeBinaryZeroNull(v, n, vec);
        break; case Encoding::EXPNORM_EXPLICIT_NULL: EncodeExpnormExplicitNull(v, vmax, p, vec);
        break; case Encoding::EXPNORM_MASKING_NULL: EncodeExpnormMaskingNull(v, vmax, p, vec);
        break; case Encoding::EXPNORM_STRICT_NULL: EncodeExpnormStrictNull(v, vmax, p, vec);
        break; case Encoding::EXPNORM_ZERO_NULL: EncodeExpnormZeroNull(v, vmax, p, vec);
        break; case Encoding::LINNORM_EXPLICIT_NULL: EncodeLinnormExplicitNull(v, vmax, vec);
        break; case Encoding::LINNORM_MASKING_NULL: EncodeLinnormMaskingNull(v, vmax, vec);
        break; case Encoding::LINNORM_STRICT_NULL: EncodeLinnormStrictNull(v, vmax, vec);
        break; case Encoding::LINNORM_ZERO_NULL: EncodeLinnormZeroNull(v, vmax, vec);
        break; case Encoding::CATEGORICAL_EXPLICIT_NULL: EncodeCategoricalExplicitNull(v, n, vec);
        break; case Encoding::CATEGORICAL_IMPLICIT_NULL: EncodeCategoricalImplicitNull(v, n, vec);
        break; case Encoding::CATEGORICAL_MASKING_NULL: EncodeCategoricalMaskingNull(v, n, vec);
        break; case Encoding::CATEGORICAL_STRICT_NULL: EncodeCategoricalStrictNull(v, n, vec);
        break; case Encoding::CATEGORICAL_ZERO_NULL: EncodeCategoricalZeroNull(v, n, vec);
        break; case Encoding::ACCUMULATING_EXPLICIT_NULL: EncodeAccumulatingExplicitNull(v, n, vec);
        break; case Encoding::ACCUMULATING_IMPLICIT_NULL: EncodeAccumulatingImplicitNull(v, n, vec);
        break; case Encoding::ACCUMULATING_MASKING_NULL: EncodeAccumulatingMaskingNull(v, n, vec);
        break; case Encoding::ACCUMULATING_STRICT_NULL: EncodeAccumulatingStrictNull(v, n, vec);
        break; case Encoding::ACCUMULATING_ZERO_NULL: EncodeAccumulatingZeroNull(v, n, vec);
        break; default:
            THROW_FORMAT("Unexpected Encoding: %d", EI(e));
        }
    }

    void Encoder::Encode(const HexAttribute a, const int v, BS &vec) {
        auto &[_, e, n, vmax, p] = HEX_ENCODING.at(EI(a));
        Encode("HexAttribute", EI(a), e, n, vmax, p, v, vec);
    }

    void Encoder::Encode(const PlayerAttribute a, const int v, BS &vec) {
        auto &[_, e, n, vmax, p] = PLAYER_ENCODING.at(EI(a));
        Encode("PlayerAttribute", EI(a), e, n, vmax, p, v, vec);
    }

    void Encoder::Encode(const GlobalAttribute a, const int v, BS &vec) {
        auto &[_, e, n, vmax, p] = GLOBAL_ENCODING.at(EI(a));
        Encode("GlobalAttribute", EI(a), e, n, vmax, p, v, vec);
    }

    //
    // ACCUMULATING
    //
    void Encoder::EncodeAccumulatingExplicitNull(const int v, const int n, BS &vec) {
        if (v == NULL_VALUE_UNENCODED) {
            vec.push_back(1);
            ADD_ZEROS_AND_RETURN(n-1, vec);
        };
        vec.push_back(0);
        EncodeAccumulating(v, n-1, vec);
    }

    void Encoder::EncodeAccumulatingImplicitNull(const int v, const int n, BS &vec) {
        if (v == NULL_VALUE_UNENCODED) {
            ADD_ZEROS_AND_RETURN(n, vec);
        }
        EncodeAccumulating(v, n, vec);
    }

    void Encoder::EncodeAccumulatingMaskingNull(const int v, const int n, BS &vec) {
        MAYBE_ADD_MASKED_AND_RETURN(v, n, vec);
        EncodeAccumulating(v, n, vec);
    }

    void Encoder::EncodeAccumulatingStrictNull(const int v, const int n, BS &vec) {
        MAYBE_THROW_STRICT_ERROR(v);
        EncodeAccumulating(v, n, vec);
    }

    void Encoder::EncodeAccumulatingZeroNull(const int v, const int n, BS &vec) {
        if (v <= 0) {
            vec.push_back(1);
            ADD_ZEROS_AND_RETURN(n-1, vec);
        }
        EncodeAccumulating(v, n, vec);
    }

    void Encoder::EncodeAccumulating(const int v, const int n, BS &vec) {
        vec.insert(vec.end(), v+1, 1);
        vec.insert(vec.end(), n-v-1, 0);
    }

    //
    // BINARY
    //

    void Encoder::EncodeBinaryExplicitNull(const int v, const int n, BS &vec) {
        vec.push_back(v == NULL_VALUE_UNENCODED);
        EncodeBinary(v, n-1, vec);
    }

    void Encoder::EncodeBinaryMaskingNull(const int v, const int n, BS &vec) {
        MAYBE_ADD_MASKED_AND_RETURN(v, n, vec);
        EncodeBinary(v, n, vec);
    }

    void Encoder::EncodeBinaryStrictNull(const int v, const int n, BS &vec) {
        MAYBE_THROW_STRICT_ERROR(v);
        EncodeBinary(v, n, vec);
    }

    void Encoder::EncodeBinaryZeroNull(const int v, const int n, BS &vec) {
        EncodeBinary(v, n, vec);
    }

    void Encoder::EncodeBinary(const int v, const int n, BS &vec) {
        MAYBE_ADD_ZEROS_AND_RETURN(v, n, vec);

        int vtmp = v;
        for (int i=0; i < n; ++i) {
            vec.push_back(vtmp % 2);
            vtmp /= 2;
        }
    }

    //
    // CATEGORICAL
    //

    void Encoder::EncodeCategoricalExplicitNull(const int v, const int n, BS &vec) {
        if (v == NULL_VALUE_UNENCODED) {
            vec.push_back(v == NULL_VALUE_UNENCODED);
            ADD_ZEROS_AND_RETURN(n-1, vec);
        }
        vec.push_back(0);
        EncodeCategorical(v, n-1, vec);
    }

    void Encoder::EncodeCategoricalImplicitNull(const int v, const int n, BS &vec) {
        if (v == NULL_VALUE_UNENCODED) {
            ADD_ZEROS_AND_RETURN(n, vec);
        }

        EncodeCategorical(v, n, vec);
    }

    void Encoder::EncodeCategoricalMaskingNull(const int v, const int n, BS &vec) {
        MAYBE_ADD_MASKED_AND_RETURN(v, n, vec);
        EncodeCategorical(v, n, vec);
    }

    void Encoder::EncodeCategoricalStrictNull(const int v, const int n, BS &vec) {
        MAYBE_THROW_STRICT_ERROR(v);
        EncodeCategorical(v, n, vec);
    }

    void Encoder::EncodeCategoricalZeroNull(const int v, const int n, BS &vec) {
        EncodeCategorical(v, n, vec);
    }

    void Encoder::EncodeCategorical(const int v, const int n, BS &vec) {
        if (v <= 0) {
            vec.push_back(1);
            ADD_ZEROS_AND_RETURN(n-1, vec);
        }

        for (int i=0; i < n; ++i) {
            if (i == v) {
                vec.push_back(1);
                ADD_ZEROS_AND_RETURN(n-i-1, vec);
            } else {
                vec.push_back(0);
            }
        }
    }

    //
    // EXPNORM
    //

    void Encoder::EncodeExpnormExplicitNull(const int v, const int vmax, double slope, BS &vec) {
        vec.push_back(v == NULL_VALUE_UNENCODED);
        EncodeExpnorm(v, vmax, slope, vec);
    }

    void Encoder::EncodeExpnormMaskingNull(const int v, const int vmax, double slope, BS &vec) {
        if (v == NULL_VALUE_UNENCODED) {
            vec.push_back(NULL_VALUE_ENCODED);
            return;
        }
        EncodeExpnorm(v, vmax, slope, vec);
    }

    void Encoder::EncodeExpnormStrictNull(const int v, const int vmax, double slope, BS &vec) {
        MAYBE_THROW_STRICT_ERROR(v);
        EncodeExpnorm(v, vmax, slope, vec);
    }

    void Encoder::EncodeExpnormZeroNull(const int v, const int vmax, double slope, BS &vec) {
        EncodeExpnorm(v, vmax, slope, vec);
    }

    void Encoder::EncodeExpnorm(const int v, const int vmax, double slope, BS &vec) {
        if (v <= 0) {
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
    float Encoder::CalcExpnorm(const int v, const int vmax, const double slope) {
        double ratio = static_cast<double>(v) / vmax;
        return std::log1p(ratio * (std::exp(slope) - 1.0)) / (slope + 1e-6);
    }

    //
    // LINNORM
    //

    void Encoder::EncodeLinnormExplicitNull(const int v, const int vmax, BS &vec) {
        vec.push_back(v == NULL_VALUE_UNENCODED);
        EncodeLinnorm(v, vmax, vec);
    }

    void Encoder::EncodeLinnormMaskingNull(const int v, const int vmax, BS &vec) {
        if (v == NULL_VALUE_UNENCODED) {
            vec.push_back(NULL_VALUE_ENCODED);
            return;
        }
        EncodeLinnorm(v, vmax, vec);
    }

    void Encoder::EncodeLinnormStrictNull(const int v, const int vmax, BS &vec) {
        MAYBE_THROW_STRICT_ERROR(v);
        EncodeLinnorm(v, vmax, vec);
    }

    void Encoder::EncodeLinnormZeroNull(const int v, const int vmax, BS &vec) {
        EncodeLinnorm(v, vmax, vec);
    }

    void Encoder::EncodeLinnorm(const int v, const int vmax, BS &vec) {
        if (v <= 0) {
            vec.push_back(0);
            return;
        }

        // XXX: this is a simplified version for 0..1 norm
        vec.push_back(CalcLinnorm(v, vmax));
    }

    float Encoder::CalcLinnorm(const int v, const int vmax) {
        return static_cast<float>(v) / static_cast<float>(vmax);
    }
}
