#include "BAI/v3/encoder.h"
#include "schema/v3/constants.h"
#include "schema/v3/types.h"
#include "test/googletest/googletest/include/gtest/gtest.h"

using Encoder = MMAI::BAI::V3::Encoder;
using namespace MMAI::Schema::V3;


TEST(Encoder, Encode) {
  {
    constexpr auto a = HexAttribute::Y_COORD;
    constexpr auto e = std::get<1>(HEX_ENCODING.at(int(a)));
    constexpr auto n = std::get<2>(HEX_ENCODING.at(int(a)));
    static_assert(e == Encoding::CATEGORICAL_STRICT_NULL, "test needs to be updated");
    static_assert(n == 11, "test needs to be updated");

    {
        auto have = std::vector<float> {};
        auto want = std::vector<float> {1,0,0,0,0,0,0,0,0,0,0};
        Encoder::Encode(a, 0, have);
        ASSERT_EQ(want, have);
    }
    {
        auto have = std::vector<float> {};
        auto want = std::vector<float> {0,0,0,0,0,0,0,0,0,1,0};
        Encoder::Encode(a, 9, have);
        ASSERT_EQ(want, have);
    }
    {
        auto have = std::vector<float> {};
        ASSERT_THROW(Encoder::Encode(a, -1, have), std::runtime_error);
    }
    {
        auto have = std::vector<float> {};
        ASSERT_THROW(Encoder::Encode(a, 666, have), std::runtime_error);
    }
  }
}

TEST(Encoder, AccumulatingExplicitNull) {
  static_assert(EI(Encoding::ACCUMULATING_EXPLICIT_NULL) == 0, "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,1,1,1,1};
    Encoder::EncodeAccumulatingExplicitNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,1,0,0,0};
    Encoder::EncodeAccumulatingExplicitNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeAccumulatingExplicitNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, AccumulatingImplicitNull) {
  static_assert(EI(Encoding::ACCUMULATING_IMPLICIT_NULL) == 1+EI(Encoding::ACCUMULATING_EXPLICIT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,1,1,0};
    Encoder::EncodeAccumulatingImplicitNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeAccumulatingImplicitNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeAccumulatingImplicitNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, AccumulatingMaskingNull) {
  static_assert(EI(Encoding::ACCUMULATING_MASKING_NULL) == 1+EI(Encoding::ACCUMULATING_IMPLICIT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,1,1,0};
    Encoder::EncodeAccumulatingMaskingNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeAccumulatingMaskingNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {-1,-1,-1,-1,-1};
    Encoder::EncodeAccumulatingMaskingNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, AccumulatingStrictNull) {
  static_assert(EI(Encoding::ACCUMULATING_STRICT_NULL) == 1+EI(Encoding::ACCUMULATING_MASKING_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,1,1,0};
    Encoder::EncodeAccumulatingStrictNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeAccumulatingStrictNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    ASSERT_THROW(Encoder::EncodeAccumulatingStrictNull(-1, 5, have), std::runtime_error);
  }
}

TEST(Encoder, AccumulatingZeroNull) {
  static_assert(EI(Encoding::ACCUMULATING_ZERO_NULL) == 1+EI(Encoding::ACCUMULATING_STRICT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,1,1,0};
    Encoder::EncodeAccumulatingZeroNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeAccumulatingZeroNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeAccumulatingZeroNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, BinaryExplicitNull) {
  static_assert(EI(Encoding::BINARY_EXPLICIT_NULL) == 1+EI(Encoding::ACCUMULATING_ZERO_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,1,1,0,0};
    Encoder::EncodeBinaryExplicitNull(0b11, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeBinaryExplicitNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeBinaryExplicitNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, BinaryMaskingNull) {
  static_assert(EI(Encoding::BINARY_MASKING_NULL) == 1+EI(Encoding::BINARY_EXPLICIT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,0,0,0};
    Encoder::EncodeBinaryMaskingNull(0b11, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeBinaryMaskingNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {-1,-1,-1,-1,-1};
    Encoder::EncodeBinaryMaskingNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, BinaryStrictNull) {
  static_assert(EI(Encoding::BINARY_STRICT_NULL) == 1+EI(Encoding::BINARY_MASKING_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,0,0,0};
    Encoder::EncodeBinaryStrictNull(0b11, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeBinaryStrictNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    ASSERT_THROW(Encoder::EncodeBinaryStrictNull(-1, 5, have), std::runtime_error);
  }
}

TEST(Encoder, BinaryZeroNull) {
  static_assert(EI(Encoding::BINARY_ZERO_NULL) == 1+EI(Encoding::BINARY_STRICT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,1,0,0,0};
    Encoder::EncodeBinaryZeroNull(0b11, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeBinaryZeroNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeBinaryZeroNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, CategoricalExplicitNull) {
  static_assert(EI(Encoding::CATEGORICAL_EXPLICIT_NULL) == 1+EI(Encoding::BINARY_ZERO_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,1};
    Encoder::EncodeCategoricalExplicitNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,1,0,0,0};
    Encoder::EncodeCategoricalExplicitNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeCategoricalExplicitNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, CategoricalImplicitNull) {
  static_assert(EI(Encoding::CATEGORICAL_IMPLICIT_NULL) == 1+EI(Encoding::CATEGORICAL_EXPLICIT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,1,0};
    Encoder::EncodeCategoricalImplicitNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeCategoricalImplicitNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,0,0};
    Encoder::EncodeCategoricalImplicitNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, CategoricalMaskingNull) {
  static_assert(EI(Encoding::CATEGORICAL_MASKING_NULL) == 1+EI(Encoding::CATEGORICAL_IMPLICIT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,1,0};
    Encoder::EncodeCategoricalMaskingNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeCategoricalMaskingNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {-1,-1,-1,-1,-1};
    Encoder::EncodeCategoricalMaskingNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, CategoricalStrictNull) {
  static_assert(EI(Encoding::CATEGORICAL_STRICT_NULL) == 1+EI(Encoding::CATEGORICAL_MASKING_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,1,0};
    Encoder::EncodeCategoricalStrictNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeCategoricalStrictNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    ASSERT_THROW(Encoder::EncodeCategoricalStrictNull(-1, 5, have), std::runtime_error);
  }
}

TEST(Encoder, CategoricalZeroNull) {
  static_assert(EI(Encoding::CATEGORICAL_ZERO_NULL) == 1+EI(Encoding::CATEGORICAL_STRICT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0,0,0,1,0};
    Encoder::EncodeCategoricalZeroNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeCategoricalZeroNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1,0,0,0,0};
    Encoder::EncodeCategoricalZeroNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, NormalizedExplicitNull) {
  static_assert(EI(Encoding::NORMALIZED_EXPLICIT_NULL) == 1+EI(Encoding::CATEGORICAL_ZERO_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0, 0.6};
    Encoder::EncodeNormalizedExplicitNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0, 0};
    Encoder::EncodeNormalizedExplicitNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {1, 0};
    Encoder::EncodeNormalizedExplicitNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, NormalizedMaskingNull) {
  static_assert(EI(Encoding::NORMALIZED_MASKING_NULL) == 1+EI(Encoding::NORMALIZED_EXPLICIT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0.6};
    Encoder::EncodeNormalizedMaskingNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0};
    Encoder::EncodeNormalizedMaskingNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {-1};
    Encoder::EncodeNormalizedMaskingNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

TEST(Encoder, NormalizedStrictNull) {
  static_assert(EI(Encoding::NORMALIZED_STRICT_NULL) == 1+EI(Encoding::NORMALIZED_MASKING_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0.6};
    Encoder::EncodeNormalizedStrictNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0};
    Encoder::EncodeNormalizedStrictNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    ASSERT_THROW(Encoder::EncodeNormalizedStrictNull(-1, 5, have), std::runtime_error);
  }
}

TEST(Encoder, NormalizedZeroNull) {
  static_assert(EI(Encoding::NORMALIZED_ZERO_NULL) == 1+EI(Encoding::NORMALIZED_STRICT_NULL), "Encoding list has changed");
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0.6};
    Encoder::EncodeNormalizedZeroNull(3, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0};
    Encoder::EncodeNormalizedZeroNull(0, 5, have);
    ASSERT_EQ(want, have);
  }
  {
    auto have = std::vector<float> {};
    auto want = std::vector<float> {0};
    Encoder::EncodeNormalizedZeroNull(-1, 5, have);
    ASSERT_EQ(want, have);
  }
}

