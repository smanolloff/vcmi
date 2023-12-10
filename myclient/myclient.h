#pragma once
#include <string>
#include <functional>

int mymain(
    std::string resdir,
    std::string mapname,
    std::string mode,
    std::string ainame,
    std::string model
);

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
