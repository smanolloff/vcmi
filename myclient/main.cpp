#include "myclient.h"
#include <cstdio>
#include <string>

int main(int argc, char * argv[])
{
  // std::function<void(int)> callback = nullptr;
  // mymain(3);

  std::string resdir("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin");

  // NOTE: the .vmap extension may be ommitted
  std::string mapname(argc > 1 ? argv[1] : "simotest.vmap");

  return mymain(resdir, mapname, false);

  // return mymain(
  //   std::string("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin"),
  //   false,
  //   callback
  // );
}

