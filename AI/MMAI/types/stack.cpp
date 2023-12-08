#include "stack.h"
#include <algorithm>

namespace MMAI {
    // static
    StackAttrs Stack::NAAttrs() {
        auto res = StackAttrs{};
        res.fill(ATTR_NA);
        return res;
    }

    // Require an explicit argument:
    // this is to prevent invoking the "default" constructor by mistake:
    // (there should be only 1 invalid stack - the global INVALID_STACK)
    Stack::Stack(bool donotuse)
    : cstack(nullptr), attrs(NAAttrs()) {};

    Stack::Stack(const CStack * cstack_, const StackAttrs attrs_)
    : cstack(cstack_), attrs(attrs_) {
        ASSERT(cstack, "null cstack");
    };
}

