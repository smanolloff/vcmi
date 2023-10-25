#include <string>
#include <functional>

// NOTE: use this to call from connector
// extern "C" int __attribute__((visibility("default"))) mymain(
//   std::string resourcedir,
//   bool headless,
//   const std::function<void(int)> &callback);

// NOTE: use this to allow breakpoints in debugger...
int mymain(int x);

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
