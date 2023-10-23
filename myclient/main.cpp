#include <cstdio>
#include "myclient.h"

int main(int argc, char * argv[])
{
  auto respath = boost::filesystem::system_complete(argv[0]).parent_path();
  return mymain(respath.string());
}

