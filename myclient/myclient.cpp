// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#include <boost/filesystem/operations.hpp>
#include <cstdio>
#include <iostream>
#include <dlfcn.h>
#include <filesystem>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>

#include "AI/MMAI/export.h"
#include "myclient.h"
#include "mmai_export.h"
#include "loader.h"

#include "../lib/filesystem/Filesystem.h"
#include "../lib/CGeneralTextHandler.h"
#include "../lib/VCMIDirs.h"
#include "../lib/VCMI_Lib.h"
#include "../lib/CConfigHandler.h"

#include "../lib/logging/CBasicLogConfigurator.h"

#include "../client/StdInc.h"
#include "../client/CGameInfo.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/logging/CBasicLogConfigurator.h"
#include "../lib/CConsoleHandler.h"
#include "../lib/VCMIDirs.h"
#include "../client/gui/CGuiHandler.h"
#include "../client/mainmenu/CMainMenu.h"
#include "../client/windows/CMessage.h"
#include "../client/CServerHandler.h"
#include "../client/CVideoHandler.h"
#include "../client/CMusicHandler.h"
#include "../client/ClientCommandManager.h"
#include "../client/gui/CursorHandler.h"
#include "../client/eventsSDL/InputHandler.h"
#include "../client/render/Graphics.h"
#include "../client/render/IScreenHandler.h"
#include "../client/CPlayerInterface.h"
#include "../client/gui/WindowHandler.h"

#include "../lib/filesystem/Filesystem.h"
#include "../lib/CGeneralTextHandler.h"
#include "../lib/VCMIDirs.h"
#include "../lib/VCMI_Lib.h"
#include "../lib/CConfigHandler.h"
#include "vstd/CLoggerBase.h"

extern boost::thread_specific_ptr<bool> inGuiThread;

static CBasicLogConfigurator *logConfig;

std::string mapname;
MMAI::Export::Baggage* baggage;
bool headless;

#ifndef VCMI_BIN_DIR
#error "VCMI_BIN_DIR compile definition needs to be set"
#endif

#if defined(VCMI_MAC)
#define LIBEXT "dylib"
#elif defined(VCMI_UNIX)
#define LIBEXT "so"
#else
#error "Unsupported OS"
#endif

//
// NOTE:
// This requires 2-player maps where player 1 is HUMAN
// The player hero moves 1 tile to the right and engages into a battle
//
// XXX:
// This will not work unless the current dir is vcmi-gym's root dir
// (the python module resolution gets messed up otherwise)
MMAI::Export::F_GetAction loadModel(MMAI::Export::Side side, std::string model, std::string gymdir) {
    auto old = boost::filesystem::current_path();
    auto gympath = boost::filesystem::canonical(boost::filesystem::path(gymdir));

    //
    // XXX: this makes it impossible to use lldb (invalid instruction error...)
    //
    auto libfile = gympath / "vcmi_gym/envs/v0/connector/build/libloader." LIBEXT;
    void* handle = dlopen(libfile.c_str(), RTLD_GLOBAL | RTLD_NOW);
    if(!handle) throw std::runtime_error("Error loading the library: " + std::string(dlerror()));

    if (side == MMAI::Export::Side::ATTACKER) {
        // can't be shared var - func pointers are too complex for me
        auto init = reinterpret_cast<decltype(&ConnectorLoader_initAttacker)>(dlsym(handle, "ConnectorLoader_initAttacker"));
        if(!init) throw std::runtime_error("Error getting init fn: " + std::string(dlerror()));

        auto getAction = reinterpret_cast<decltype(&ConnectorLoader_getActionAttacker)>(dlsym(handle, "ConnectorLoader_getActionAttacker"));
        if(!getAction) throw std::runtime_error("Error getting getAction fn: " + std::string(dlerror()));

        init(gymdir, model);
        boost::filesystem::current_path(old);
        return getAction;
    }

    // defender
    auto init = reinterpret_cast<decltype(&ConnectorLoader_initDefender)>(dlsym(handle, "ConnectorLoader_initDefender"));
    if(!init) throw std::runtime_error("Error getting init fn: " + std::string(dlerror()));

    auto getAction = reinterpret_cast<decltype(&ConnectorLoader_getActionDefender)>(dlsym(handle, "ConnectorLoader_getActionDefender"));
    if(!getAction) throw std::runtime_error("Error getting getAction fn: " + std::string(dlerror()));

    init(gymdir, model);
    boost::filesystem::current_path(old);
    return getAction;
};

void validateValue(std::string name, std::string value, std::vector<std::string> values) {
    if (std::find(values.begin(), values.end(), value) != values.end())
        return;
    std::cerr << "Bad value for " << name << ": " << value << "\n";
    exit(1);
}

void validateFile(std::string name, std::string path, boost::filesystem::path wd) {
    auto p = boost::filesystem::path(path);

    if (p.is_absolute()) {
        if (boost::filesystem::is_regular_file(path))
            return;

        std::cerr << "Bad value for " << name << ": " << path << "\n";
    } else {
        if (boost::filesystem::is_regular_file(wd / path))
            return;

        std::cerr << "Bad value for " << name << ": " << path << "\n";
        std::cerr << "(relative to: " << wd.string() << ")\n";
    }

    exit(1);
}

void validateArguments(
    std::string &gymdir,
    std::string &map,
    std::string &loglevelGlobal,
    std::string &loglevelAI,
    std::string &attackerAI,
    std::string &defenderAI,
    std::string &attackerModel,
    std::string &defenderModel
) {
    auto wd = boost::filesystem::current_path();

    validateValue("attackerAI", attackerAI, AIS);
    validateValue("defenderAI", defenderAI, AIS);

    if (attackerAI == AI_MMAI_MODEL)
        validateFile("attackerModel", attackerModel, wd);

    if (defenderAI == AI_MMAI_MODEL)
        validateFile("defenderModel", defenderModel, wd);

    // XXX: this might blow up since preinitDLL is not yet called here
    validateFile("map", map, VCMIDirs::get().userDataPath() / "Maps");

    if (boost::filesystem::is_directory(VCMI_BIN_DIR)) {
        if (!boost::filesystem::is_regular_file(boost::filesystem::path(VCMI_BIN_DIR) / "AI" / "libMMAI." LIBEXT)) {
            std::cerr << "Bad value for VCMI_BIN_DIR: exists, but AI/libMMAI." LIBEXT " was not found: " << VCMI_BIN_DIR << "\n";
            exit(1);
        }
    } else {
        std::cerr << "Bad value for VCMI_BIN_DIR: " << VCMI_BIN_DIR << "\n(not a directory)\n";
            exit(1);
    }

    if (boost::filesystem::is_directory(gymdir)) {
        if (!boost::filesystem::is_regular_file(boost::filesystem::path(gymdir) / "vcmi_gym" / "__init__.py")) {
            std::cerr << "Bad value for gymdir: exists, but vcmi_gym/__init__.py was not found: " << gymdir << "\n";
            exit(1);
        }
    } else {
        std::cerr << "Bad value for gymdir: not a directory: " << gymdir << "\n(check --gymdir option)\n";
        exit(1);
    }

    // if (headless && attackerAI != AI_MMAI_MODEL && attackerAI != AI_MMAI_USER) {
    //     std::cerr << "headless mode requires an MMAI-type attackerAI\n";
    //     exit(1);
    // }

    if (!baggage)
        throw std::runtime_error("baggage is required");

    if (!baggage->f_getAction)
        throw std::runtime_error("baggage->f_getAction is required");

    if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), loglevelAI) == LOGLEVELS.end()) {
        std::cerr << "Bad value for loglevelAI: " << loglevelAI << "\n";
        exit(1);
    }

    if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), loglevelGlobal) == LOGLEVELS.end()) {
        std::cerr << "Bad value for loglevelGlobal: " << loglevelGlobal << "\n";
        exit(1);
    }
}

void processArguments(
    std::string &gymdir,
    std::string &map,
    std::string &loglevelGlobal,
    std::string &loglevelAI,
    std::string &attackerAI,
    std::string &defenderAI,
    std::string &attackerModel,
    std::string &defenderModel,
    int randomCombat
) {
    // Notes on AI creation
    //
    //
    // *** Game start - adventure interfaces ***
    // initGameInterface() is called on:
    // * CPlayerInterface (CPI) for humans
    // * settings["playerAI"] for computers
    //   settings["playerAI"] is fixed to "MMAI" (ie. MMAI::AAI), which
    //   can create battle interfaces as per `attackerAI` and `defenderAI`
    //   script arguments (this info is passed to AAI via baggage).
    //
    // *** Battle start - battle interfaces ***
    // * battleStart() is called on the adventure interfaces for all players
    //
    // VCAI - (via parent class) reads settings["enemyAI"]) and calls GetNewBattleAI()
    // MMAI - creates BAI directly, passing the user's getAction fn via baggage
    // CPI - reads settings["friendlyAI"], but modified to init it with baggage
    //       (baggage ignored by non-MMAIs)
    //
    // *** Auto-combat button clicked ***
    // BattleWindow()::bAutofightf() has been patched to reuse code in CPI
    // (in order to also pass baggage)
    //
    // Notes on AI deletion
    // *** Battle end ***
    // The battleEnd() is sent to the ADVENTURE AI:
    // * for CPI, it's NOT forwarded to the battle AI (which is just destroyed)
    // * for MMAI, it's forwarded to the battle AI and THEN it gets destroyed

    //
    // AI for attacker
    // When attacker is "MMAI":
    //  * if headless=true, VCMI will create MMAI::AAI which inits BAI via myInitBattleInterface()
    //  * if headless=false, it will create CPI which inits BAI via the (default) initBattleInterface()
    //
    // CPI creates battle interfaces as per settings["friendlyAI"]
    // => must set that setting also (see further down)
    // (Note: CPI had to be modded to pass the baggage)
    //
    if (attackerAI == AI_MMAI_USER) {
        baggage->attackerBattleAIName = "MMAI";
    } else if (attackerAI == AI_MMAI_MODEL) {
        baggage->attackerBattleAIName = "MMAI";
        // Same as above, but with replaced "getAction" for attacker
        baggage->f_getActionAttacker = loadModel(MMAI::Export::Side::ATTACKER, attackerModel, gymdir);
    } else if (attackerAI == AI_STUPIDAI) {
        baggage->attackerBattleAIName = "StupidAI";
    } else if (attackerAI == AI_BATTLEAI) {
        baggage->attackerBattleAIName = "BattleAI";
    } else {
        throw std::runtime_error("Unexpected attackerAI: " + attackerAI);
    }

    //
    // AI for defender (aka. computer, aka. some AI)
    // Defender is computer with adventure interface as per settings["playerAI"]
    // (ie. always MMAI::AAI)
    //
    // * "MMAI", which will create BAI/StupidAI/BattleAI battle interfaces
    //   based on info provided via baggage.
    //
    if (defenderAI == AI_MMAI_USER) {
        baggage->defenderBattleAIName = "MMAI";
    } else if (defenderAI == AI_MMAI_MODEL) {
        baggage->defenderBattleAIName = "MMAI";
        // Same as above, but with replaced "getAction" for defender
        baggage->f_getActionDefender = loadModel(MMAI::Export::Side::DEFENDER, defenderModel, gymdir);
    } else if (defenderAI == AI_STUPIDAI) {
        baggage->defenderBattleAIName = "StupidAI";
    } else if (defenderAI == AI_BATTLEAI) {
        baggage->defenderBattleAIName = "BattleAI";
    } else {
        throw std::runtime_error("Unexpected defenderAI: " + defenderAI);
    }

    // All adventure AIs must be MMAI to properly init the battle AIs
    Settings(settings.write({"server", "playerAI"}))->String() = "MMAI";
    Settings(settings.write({"server", "oneGoodAI"}))->Bool() = false;

    Settings(settings.write({"session", "headless"}))->Bool() = headless;
    Settings(settings.write({"session", "onlyai"}))->Bool() = headless;
    Settings(settings.write({"adventure", "quickCombat"}))->Bool() = headless;
    Settings(settings.write({"server", "randomCombat"}))->Integer() = randomCombat;

    // CPI needs this setting in case the attacker is human (headless==false)
    Settings(settings.write({"server", "friendlyAI"}))->String() = baggage->attackerBattleAIName;

    // convert to "ai/simotest.vmap" to "maps/ai/simotest.vmap"
    auto mappath = std::filesystem::path("Maps") / std::filesystem::path(map);
    // store "maps/ai/simotest.vmap" into global var
    mapname = mappath.string();

    // Set "lastMap" to prevent some race condition debugStartTest+Menu screen
    // convert to "maps/ai/simotest.vmap" to "maps/ai/simotest"
    auto lastmap = (mappath.parent_path() / mappath.stem()).string();
    // convert to "maps/ai/simotest" to "MAPS/AI/SIMOTEST"
    std::transform(lastmap.begin(), lastmap.end(), lastmap.begin(), [](unsigned char c) { return std::toupper(c); });
    Settings(settings.write({"general", "lastMap"}))->String() = lastmap;

    //
    // Configure logging
    //
    Settings loggers = settings.write["logging"]["loggers"];
    loggers->Vector().clear();

    auto conflog = [&loggers](std::string domain, std::string lvl) {
        JsonNode jlog, jlvl, jdomain;
        jdomain.String() = domain;
        jlvl.String() = lvl;
        jlog.Struct() = std::map<std::string, JsonNode>{{"level", jlvl}, {"domain", jdomain}};
        loggers->Vector().push_back(jlog);
    };

    conflog("global", loglevelGlobal);
    conflog("ai", loglevelAI);

    // conflog("global", "trace");
    // conflog("ai", "trace");
    // conflog("network", "trace");
    // conflog("mod", "error");
    // conflog("animation", "error");
}

void init_vcmi(
    MMAI::Export::Baggage* baggage_,
    std::string gymdir,
    std::string map,
    int randomCombat,
    std::string loglevelGlobal,
    std::string loglevelAI,
    std::string attackerAI,
    std::string defenderAI,
    std::string attackerModel,
    std::string defenderModel,
    bool printBattleResults,
    bool headless_
) {
    // SIGSEGV errors if this is not global
    baggage = baggage_;
    baggage->printBattleResults = printBattleResults;
    baggage->map = map;

    // this is used in start_vcmi()
    headless = headless_;

    validateArguments(
        gymdir,
        map,
        loglevelGlobal,
        loglevelAI,
        attackerAI,
        defenderAI,
        attackerModel,
        defenderModel
    );

    boost::filesystem::current_path(boost::filesystem::path(VCMI_BIN_DIR));
    std::cout.flags(std::ios::unitbuf);
    console = new CConsoleHandler();

    auto callbackFunction = [](std::string buffer, bool calledFromIngameConsole)
    {
        ClientCommandManager commandController;
        commandController.processCommand(buffer, calledFromIngameConsole);
    };

    *console->cb = callbackFunction;
    console->start();

    const boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
    logConfig = new CBasicLogConfigurator(logPath, console);
    logConfig->configureDefault();

    // XXX: apparently this needs to be invoked before Settings() stuff
    preinitDLL(::console);

    processArguments(
        gymdir,
        map,
        loglevelGlobal,
        loglevelAI,
        attackerAI,
        defenderAI,
        attackerModel,
        defenderModel,
        randomCombat
    );

    // printf("gymdir: %s\n", gymdir.c_str());
    // printf("map: %s\n", map.c_str());
    // printf("loglevelGlobal: %s\n", loglevelGlobal.c_str());
    // printf("loglevelAI: %s\n", loglevelAI.c_str());
    // printf("attackerAI: %s\n", attackerAI.c_str());
    // printf("defenderAI: %s\n", defenderAI.c_str());
    // printf("attackerModel: %s\n", attackerModel.c_str());
    // printf("defenderModel: %s\n", defenderModel.c_str());
    // printf("headless: %d\n", headless);

    Settings(settings.write({"battle", "speedFactor"}))->Integer() = 5;
    Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
    Settings(settings.write({"battle", "stickyHeroInfoWindows"}))->Bool() = false;
    Settings(settings.write({"logging", "console", "format"}))->String() = "[%t][%n] %l %m";
    Settings(settings.write({"logging", "console", "coloredOutputEnabled"}))->Bool() = true;

    Settings colors = settings.write["logging"]["console"]["colorMapping"];
    colors->Vector().clear();

    auto confcolor = [&colors](std::string domain, std::string lvl, std::string color) {
        JsonNode jentry, jlvl, jdomain, jcolor;
        jdomain.String() = domain;
        jlvl.String() = lvl;
        jcolor.String() = color;
        jentry.Struct() = std::map<std::string, JsonNode>{{"level", jlvl}, {"domain", jdomain}, {"color", jcolor}};
        colors->Vector().push_back(jentry);
    };

    confcolor("global", "trace", "gray");
    confcolor("ai",     "trace", "gray");
    confcolor("global", "debug", "gray");
    confcolor("ai",     "debug", "gray");
    confcolor("global", "info", "white");
    confcolor("ai",     "info", "white");
    confcolor("global", "warn", "yellow");
    confcolor("ai",     "warn", "yellow");
    confcolor("global", "error", "red");
    confcolor("ai",     "error", "red");

    logConfig->configure();
    // logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

    srand ( (unsigned int)time(nullptr) );

    if (!headless)
        GH.init();

    CCS = new CClientState();
    CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
    CSH = new CServerHandler(std::make_any<MMAI::Export::Baggage*>(baggage));

    if (!headless) {
        CCS->videoh = new CEmptyVideoPlayer();
        CCS->soundh = new CSoundHandler();
        CCS->soundh->init();
        CCS->soundh->setVolume((ui32)settings["general"]["sound"].Float());
        CCS->musich = new CMusicHandler();
        CCS->musich->init();
        CCS->musich->setVolume((ui32)settings["general"]["music"].Float());
    }

    boost::thread loading([]() {
        loadDLLClasses();
        const_cast<CGameInfo*>(CGI)->setFromLib();
    });
    loading.join();

    if (!headless) {
        graphics = new Graphics(); // should be before curh
        CCS->curh = new CursorHandler();
        CMessage::init();
        CCS->curh->show();
    }
}

void start_vcmi() {
    if (mapname == "")
        throw std::runtime_error("call init_vcmi first");

    logGlobal->info("friendlyAI -> " + settings["server"]["friendlyAI"].String());
    logGlobal->info("playerAI -> " + settings["server"]["playerAI"].String());
    logGlobal->info("enemyAI -> " + settings["server"]["enemyAI"].String());
    logGlobal->info("headless -> " + std::to_string(settings["session"]["headless"].Bool()));
    logGlobal->info("onlyai -> " + std::to_string(settings["session"]["onlyai"].Bool()));
    logGlobal->info("quickCombat -> " + std::to_string(settings["adventure"]["quickCombat"].Bool()));

    auto t = boost::thread(&CServerHandler::debugStartTest, CSH, mapname, false);
    inGuiThread.reset(new bool(true));

    if(headless) {
        while (true) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        }
    } else {
        GH.screenHandler().clearScreen();
        while(true) {
            GH.input().fetchEvents();
            CSH->applyPacksOnLobbyScreen();
            GH.renderFrame();
        }
    }
}
