#include "main.h"

int main(int argc, char * argv[]) {
    auto [
        baggage_,
        gymdir,
        map,
        loglevelGlobal,
        loglevelAI,
        attackerAI,
        defenderAI,
        attackerModel,
        defenderModel
    ] = parse_args(argc, argv);

    init_vcmi(
        baggage_,
        gymdir,
        map,
        loglevelGlobal,
        loglevelAI,
        attackerAI,
        defenderAI,
        attackerModel,
        defenderModel,
        true
    );

    start_vcmi();
    return 0;
}

