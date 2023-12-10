#include "myclient.h"
#include <cstdio>
#include <string>

int main(int argc, char * argv[])
{
  if (argc < 4) {
    printf("Usage: %s <map> <mode> <AI> [model]\n\n", argv[0]);
    printf("Supported modes:\n");
    printf("  mode=1 => enemy AI is StupidAI\n");
    printf("  mode=2 => enemy AI is BattleAI\n\n");
    printf("  mode=3 => enemy AI is also <AI>\n\n");
    printf("Supported AIs: MMAI, StupidAI, BattleAI\n");
    exit(1);
  }

  std::string resdir("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin");
  std::string mapname(argv[1]); // eg. "ai/M8.vmap"
  std::string mode(argv[2]); // "mode=1", "mode=2"
  std::string ainame(argv[3]); // "MMAI", "StupidAI", "BattleAI"

  if (mode != "mode=1" && mode != "mode=2" && mode != "mode=3") {
    printf("mode should be one of: mode=1, mode=2, mode=3 got: %s\n", mode.c_str());
    exit(1);
  }

  if (ainame != "MMAI" && ainame != "StupidAI" && ainame != "BattleAI") {
    printf("AI should be one of: MMAI, StupidAI, BattleAI, got: %s\n", ainame.c_str());
    exit(1);
  }

  std::string model = "";

  if (ainame == "MMAI") {
    if (argc < 4) {
      printf("MMAI also requires a model arg\n");
      exit(1);
    }
    model = argv[4];
  }

  return mymain(resdir, mapname, mode, ainame, model);

  // return mymain(
  //   std::string("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin"),
  //   false,
  //   callback
  // );
}

