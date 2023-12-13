#pragma once

#include "AI/MMAI/export.h"
#include "myclient.h"

using Args = std::tuple<
    MMAI::Export::Baggage*,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string,
    std::string
>;

Args parse_args(int argc, char * argv[]);
