#include "StdInc.h"
#include "../../lib/AI_Base.h"
#include "MMAI.h"

#ifdef __GNUC__
#define strcpy_s(a, b, c) strncpy(a, c, b)
#endif

static const char * g_cszAiName = "MMAI";

extern "C" DLL_EXPORT int GetGlobalAiVersion() {
    return AI_INTERFACE_VER;
}

extern "C" DLL_EXPORT void GetAiName(char * name) {
    strcpy_s(name, strlen(g_cszAiName) + 1, g_cszAiName);
}

extern "C" DLL_EXPORT void GetNewAI(std::shared_ptr<CGlobalAI> & out) {
    out = std::make_shared<MMAI::AAI>();
}

extern "C" DLL_EXPORT void GetNewBattleAI(std::shared_ptr<CBattleGameInterface> &out) {
    out = std::make_shared<MMAI::BAI>();
}
