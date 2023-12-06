#include "myclient.h"
#include <cstdio>
#include <string>

int main(int argc, char * argv[])
{
  if (argc < 3) {
    printf("Usage: %s <map> <AI> [model]\n\nSupported AIs: MMAI, StupidAI, BattleAI\nNOTE: MMAI also requires a model arg", argv[0]);
    exit(1);
  }

  std::string resdir("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin");
  std::string mapname(argv[1]); // eg. "ai/M8.vmap"
  std::string ainame(argv[2]); // "MMAI", "StupidAI", "BattleAI"

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
    model = argv[3];
  }

  return mymain(resdir, mapname, ainame, model);

  // return mymain(
  //   std::string("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin"),
  //   false,
  //   callback
  // );
}

