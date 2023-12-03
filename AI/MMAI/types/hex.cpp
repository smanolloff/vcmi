#include "types/hex.h"

namespace MMAI {
    // static
    int Hex::calcId(const BattleHex &bh) {
        ASSERT(bh.isAvailable(), "Hex unavailable: " + std::to_string(bh.hex));
        return bh.getX()-1 + bh.getY()*BF_XMAX;
    }

    std::string Hex::name() const {
        return "(" + std::to_string(1 + id%BF_XMAX) + "," + std::to_string(1 + id/BF_XMAX) + ")";
    }
}
