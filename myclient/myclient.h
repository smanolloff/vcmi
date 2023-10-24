#include <string>
#include <functional>

extern "C" int __attribute__((visibility("default"))) mymain(
  std::string resourcedir,
  bool headless,
  const std::function<void(int)> &callback);

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
