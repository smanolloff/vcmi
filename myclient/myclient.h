#include <string>
#include <functional>

// extern "C" int __attribute__((visibility("default"))) mymain(
//   std::string resourcedir,
//   const std::function<void(int)> &callback);

// extern "C" int __attribute__((visibility("default"))) mymain(std::string resourcedir);
extern "C" int __attribute__((visibility("default"))) mymain(int i);

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
