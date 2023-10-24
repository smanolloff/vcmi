#include "myclient.h"
#include <cstdio>
#include <string>

int main(int argc, char * argv[])
{
  std::function<void(int)> callback = nullptr;

  return mymain(
    std::string("/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin"),
    false,
    callback
  );
}

