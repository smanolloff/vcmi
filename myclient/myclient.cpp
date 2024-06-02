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

#include <ATen/core/enum_tag.h>
#include <ATen/core/ivalue.h>
#include <boost/filesystem/operations.hpp>
#include <c10/core/SymFloat.h>
#include <cstdio>
#include <iostream>
#include <dlfcn.h>
#include <filesystem>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>

#include "AI/MMAI/export.h"
#include "ExceptionsCommon.h"
#include "myclient.h"
#include "mmai_export.h"

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

#include <c10/core/ScalarType.h>
#include <torch/torch.h>
#include <torch/script.h>

static CBasicLogConfigurator *logConfig;

static std::optional<std::string> criticalInitializationError;

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

/*
[[noreturn]] static void quitApplication()
{
    CSH->endNetwork();

    if(!settings["session"]["headless"].Bool())
    {
        if(CSH->client)
            CSH->endGameplay();

        GH.windows().clear();
    }

    vstd::clear_pointer(CSH);

    if(!settings["session"]["headless"].Bool())
    {
        // cleanup, mostly to remove false leaks from analyzer
        if(CCS)
        {
            CCS->musich->release();
            CCS->soundh->release();

            delete CCS->consoleh;
            delete CCS->curh;
            delete CCS->videoh;
            delete CCS->musich;
            delete CCS->soundh;

            vstd::clear_pointer(CCS);
        }
        CMessage::dispose();

        vstd::clear_pointer(graphics);
    }

    vstd::clear_pointer(VLC);

    // sometimes leads to a hang. TODO: investigate
    //vstd::clear_pointer(console);// should be removed after everything else since used by logging

    if(!settings["session"]["headless"].Bool())
        GH.screenHandler().close();

    if(logConfig != nullptr)
    {
        logConfig->deconfigure();
        delete logConfig;
        logConfig = nullptr;
    }

    std::cout << "Ending...\n";
    exit(1);
}
*/

std::pair<MMAI::Export::F_GetAction, MMAI::Export::F_GetValue> loadModel(std::string modelPath, bool floatEncoding, bool printModelPredictions) {
    c10::InferenceMode guard;
    torch::jit::script::Module model = torch::jit::load(modelPath);
    model.eval();

    std::cout << "Loaded " << modelPath << "\n";
    auto size = MMAI::Export::STATE_SIZE_DEFAULT;
    auto sizeOneHex = MMAI::Export::STATE_SIZE_DEFAULT_ONE_HEX;

    if (floatEncoding) {
        size = MMAI::Export::STATE_SIZE_FLOAT;
        sizeOneHex = MMAI::Export::STATE_SIZE_FLOAT_ONE_HEX;
    }

    auto getvalue = [guard, model, size, sizeOneHex, floatEncoding](const MMAI::Export::Result * r) {
        auto state = MMAI::Export::State{};
        state.reserve(size);

        for (auto &u : r->stateUnencoded)
            floatEncoding ? state.push_back(u.encode2Floating()) : u.encode(state);

        // TODO: see if from_blob can directly accept the correct shape
        auto obs = torch::from_blob(state.data(), {static_cast<long>(state.size())}, torch::kFloat).reshape({11, 15, sizeOneHex});
        auto method = model.get_method("get_value");
        auto inputs = std::vector<torch::IValue>{obs};
        auto res = method(inputs).toDouble();
        return res;
    };

    auto getaction = [guard, model, size, floatEncoding, sizeOneHex, printModelPredictions](const MMAI::Export::Result * r) {
        if (r->ended)
            return MMAI::Export::ACTION_RESET;

        auto state = MMAI::Export::State{};
        state.reserve(size);

        for (auto &u : r->stateUnencoded)
            floatEncoding ? state.push_back(u.encode2Floating()) : u.encode(state);

        // TODO: see if from_blob can directly accept the correct shape
        // XXX: handle FLOAT encoding
        auto obs = torch::from_blob(state.data(), {static_cast<long>(state.size())}, torch::kFloat).reshape({11, 15, sizeOneHex});

        auto intmask = std::array<int, MMAI::Export::N_ACTIONS - 1>{};
        for (int i=0; i < intmask.size(); i++)
            intmask.at(i) = static_cast<int>(r->actmask.at(i+1));

        auto mask = torch::from_blob(intmask.data(), {static_cast<long>(intmask.size())}, torch::kInt).to(torch::kBool);

        // auto mask_accessor = mask.accessor<bool,1>();
        // for (int i = 0; i < mask_accessor.size(0); ++i)
        //     printf("mask[%d]=%d\n", i, mask_accessor[i]);

        auto method = model.get_method("predict");
        auto inputs = std::vector<torch::IValue>{obs, mask};
        auto res = method(inputs).toInt() + 1; // 1 is action offset

        if (printModelPredictions) {
            printf("AI action prediction: %d\n", int(res));

            // Also esitmate value
            auto vmethod = model.get_method("get_value");
            auto vinputs = std::vector<torch::IValue>{obs};
            auto vres = vmethod(vinputs).toDouble();
            printf("AI value estimation: %f\n", vres);
        }

        return MMAI::Export::Action(res);
    };

    return {getaction, getvalue};
}

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
    std::string &stateEncoding,
    std::string &map,
    int &maxBattles,
    int &seed,
    int &randomHeroes,
    int &randomObstacles,
    int &swapSides,
    std::string &loglevelGlobal,
    std::string &loglevelAI,
    std::string &loglevelStats,
    std::string &redAI,
    std::string &blueAI,
    std::string &redModel,
    std::string &blueModel,
    std::string &statsMode,
    std::string &statsStorage,
    int &statsPersistFreq,
    int &statsSampling,
    float &statsScoreVar,
    bool &_printModelPredictions
) {
    if (stateEncoding != MMAI::Export::STATE_ENCODING_DEFAULT && stateEncoding != MMAI::Export::STATE_ENCODING_FLOAT)
        throw std::runtime_error("Invalid state encoding: " + stateEncoding);

    auto wd = boost::filesystem::current_path();

    validateValue("redAI", redAI, AIS);
    validateValue("blueAI", blueAI, AIS);

    if (redAI == AI_MMAI_MODEL)
        validateFile("redModel", redModel, wd);

    if (blueAI == AI_MMAI_MODEL)
        validateFile("blueModel", blueModel, wd);

    // XXX: this might blow up since preinitDLL is not yet called here
    validateFile("map", map, VCMIDirs::get().userDataPath() / "Maps");

    if (statsMode != "disabled" && statsMode != "red" && statsMode != "blue") {
        std::cerr << "Bad value for statsMode: expected disabled|red|blue, got: " << statsMode << "\n";
        exit(1);
    }

    if (statsStorage != "-") {
        auto dir = std::filesystem::path(statsStorage).parent_path();
        if (!std::filesystem::is_directory(dir)) {
            std::cerr << "Bad value for statsStorage: parent is not a directory: " << dir << "\n";
            exit(1);
        }
    }

    if (maxBattles < 0) {
        std::cerr << "Bad value for maxBattles: expected a non-negative integer, got: " << maxBattles << "\n";
        exit(1);
    }

    if (randomHeroes < 0) {
        std::cerr << "Bad value for randomHeroes: expected a non-negative integer, got: " << randomHeroes << "\n";
        exit(1);
    }

    if (swapSides < 0) {
        std::cerr << "Bad value for swapSides: expected a non-negative integer, got: " << swapSides << "\n";
        exit(1);
    }

    if (statsPersistFreq < 0) {
        std::cerr << "Bad value for statsPersistFreq: expected a non-negative integer, got: " << statsPersistFreq << "\n";
        exit(1);
    }

    if (statsSampling < 0) {
        std::cerr << "Bad value for statsSampling: expected a non-negative integer, got: " << statsSampling << "\n";
        exit(1);
    }

    if (statsScoreVar < 0.01 or statsScoreVar > 0.99) {
        std::cerr << "Bad value for statsScoreVar: expected a float between 0.1 and 0.4, got: " << statsScoreVar << "\n";
        exit(1);
    }

    if (boost::filesystem::is_directory(VCMI_BIN_DIR)) {
        if (!boost::filesystem::is_regular_file(boost::filesystem::path(VCMI_BIN_DIR) / "AI" / "libMMAI." LIBEXT)) {
            std::cerr << "Bad value for VCMI_BIN_DIR: exists, but AI/libMMAI." LIBEXT " was not found: " << VCMI_BIN_DIR << "\n";
            exit(1);
        }
    } else {
        std::cerr << "Bad value for VCMI_BIN_DIR: " << VCMI_BIN_DIR << "\n(not a directory)\n";
            exit(1);
    }

    // if (headless && redAI != AI_MMAI_MODEL && redAI != AI_MMAI_USER) {
    //     std::cerr << "headless mode requires an MMAI-type redAI\n";
    //     exit(1);
    // }

    if (!baggage)
        throw std::runtime_error("baggage is required");

    if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), loglevelAI) == LOGLEVELS.end()) {
        std::cerr << "Bad value for loglevelAI: " << loglevelAI << "\n";
        exit(1);
    }

    if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), loglevelGlobal) == LOGLEVELS.end()) {
        std::cerr << "Bad value for loglevelGlobal: " << loglevelGlobal << "\n";
        exit(1);
    }

    if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), loglevelStats) == LOGLEVELS.end()) {
        std::cerr << "Bad value for loglevelStats: " << loglevelStats << "\n";
        exit(1);
    }
}

void processArguments(
    std::string stateEncoding,
    std::string &map,
    std::string &loglevelGlobal,
    std::string &loglevelAI,
    std::string &loglevelStats,
    std::string &redAI,
    std::string &blueAI,
    std::string &redModel,
    std::string &blueModel,
    int maxBattles,
    int seed,
    int randomHeroes,
    int randomObstacles,
    int swapSides,
    std::string statsMode,
    std::string statsStorage,
    int statsPersistFreq,
    int statsSampling,
    float statsScoreVar,
    bool printModelPredictions
) {
    // Notes on AI creation
    //
    //
    // *** Game start - adventure interfaces ***
    // initGameInterface() is called on:
    // * CPlayerInterface (CPI) for humans
    // * settings["playerAI"] for computers
    //   settings["playerAI"] is fixed to "MMAI" (ie. MMAI::AAI), which
    //   can create battle interfaces as per `redAI` and `blueAI`
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
    // AI for attacker (red)
    // When attacker is "MMAI":
    //  * if headless=true, VCMI will create MMAI::AAI which inits BAI via myInitBattleInterface()
    //  * if headless=false, it will create CPI which inits BAI via the (default) initBattleInterface()
    //
    // CPI creates battle interfaces as per settings["friendlyAI"]
    // => must set that setting also (see further down)
    // (Note: CPI had to be modded to pass the baggage)
    //
    if (redAI == AI_MMAI_USER) {
        baggage->battleAINameRed = "MMAI";
    } else if (redAI == AI_MMAI_MODEL) {
        baggage->battleAINameRed = "MMAI";
        // Same as above, but with replaced "getAction" for attacker
        auto [getaction, getvalue] = loadModel(redModel, stateEncoding == "float", printModelPredictions);
        baggage->f_getActionRed = getaction;
        baggage->f_getValueRed = getvalue;
    } else if (redAI == AI_STUPIDAI) {
        baggage->battleAINameRed = "StupidAI";
    } else if (redAI == AI_BATTLEAI) {
        baggage->battleAINameRed = "BattleAI";
    } else {
        throw std::runtime_error("Unexpected redAI: " + redAI);
    }

    //
    // AI for defender (aka. computer, aka. some AI)
    // Defender is computer with adventure interface as per settings["playerAI"]
    // (ie. always MMAI::AAI)
    //
    // * "MMAI", which will create BAI/StupidAI/BattleAI battle interfaces
    //   based on info provided via baggage.
    //
    if (blueAI == AI_MMAI_USER) {
        baggage->battleAINameBlue = "MMAI";
    } else if (blueAI == AI_MMAI_MODEL) {
        baggage->battleAINameBlue = "MMAI";
        // Same as above, but with replaced "getAction" for defender
        auto [getaction, getvalue] = loadModel(blueModel, stateEncoding == "float", printModelPredictions);
        baggage->f_getActionBlue = getaction;
        baggage->f_getValueBlue = getvalue;
    } else if (blueAI == AI_STUPIDAI) {
        baggage->battleAINameBlue = "StupidAI";
    } else if (blueAI == AI_BATTLEAI) {
        baggage->battleAINameBlue = "BattleAI";
    } else {
        throw std::runtime_error("Unexpected blueAI: " + blueAI);
    }

    // All adventure AIs must be MMAI to properly init the battle AIs
    Settings(settings.write({"server", "playerAI"}))->String() = "MMAI";
    Settings(settings.write({"server", "oneGoodAI"}))->Bool() = false;

    Settings(settings.write({"session", "headless"}))->Bool() = headless;
    Settings(settings.write({"session", "onlyai"}))->Bool() = headless;
    Settings(settings.write({"adventure", "quickCombat"}))->Bool() = headless;
    Settings(settings.write({"server", "maxBattles"}))->Integer() = maxBattles;
    Settings(settings.write({"server", "seed"}))->Integer() = seed;
    Settings(settings.write({"server", "randomHeroes"}))->Integer() = randomHeroes;
    Settings(settings.write({"server", "randomObstacles"}))->Integer() = randomObstacles;
    Settings(settings.write({"server", "swapSides"}))->Integer() = swapSides;
    Settings(settings.write({"server", "statsMode"}))->String() = statsMode;
    Settings(settings.write({"server", "statsStorage"}))->String() = statsStorage;
    Settings(settings.write({"server", "statsPersistFreq"}))->Integer() = statsPersistFreq;
    Settings(settings.write({"server", "statsSampling"}))->Integer() = statsSampling;
    Settings(settings.write({"server", "statsScoreVar"}))->Float() = statsScoreVar;
    Settings(settings.write({"server", "statsLoglevel"}))->String() = loglevelStats;

    Settings(settings.write({"server", "localPort"}))->Integer() = 0;
    Settings(settings.write({"server", "useProcess"}))->Bool() = false;

    // CPI needs this setting in case the attacker is human (headless==false)
    Settings(settings.write({"server", "friendlyAI"}))->String() = baggage->battleAINameRed;

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

    // XXX: stats logger can't be set here (client) as it runs on the server
    //      where it's configured via Settings in CVCMIServer.cpp#create
    // conflog("stats", "info");

    conflog("network", "error");
    conflog("mod", "error");
    conflog("animation", "error");
    conflog("bonus", "error");
}

void init_vcmi(
    MMAI::Export::Baggage* baggage_,
    std::string stateEncoding,
    std::string map,
    int maxBattles,
    int seed,
    int randomHeroes,
    int randomObstacles,
    int swapSides,
    std::string loglevelGlobal,
    std::string loglevelAI,
    std::string loglevelStats,
    std::string redAI,
    std::string blueAI,
    std::string redModel,
    std::string blueModel,
    std::string statsMode,
    std::string statsStorage,
    int statsPersistFreq,
    int statsSampling,
    float statsScoreVar,
    bool printModelPredictions,
    bool headless_
) {
    // SIGSEGV errors if this is not global
    baggage = baggage_;
    baggage->map = map;

    if (statsStorage != "-")
        statsStorage = std::filesystem::absolute(std::filesystem::path(statsStorage)).string();

    // this is used in start_vcmi()
    headless = headless_;

    validateArguments(
        stateEncoding,
        map,
        maxBattles,
        seed,
        randomHeroes,
        randomObstacles,
        swapSides,
        loglevelGlobal,
        loglevelAI,
        loglevelStats,
        redAI,
        blueAI,
        redModel,
        blueModel,
        statsMode,
        statsStorage,
        statsPersistFreq,
        statsSampling,
        statsScoreVar,
        printModelPredictions
    );

    auto wd = boost::filesystem::current_path();

    // chdir needed for VCMI init
    boost::filesystem::current_path(boost::filesystem::path(VCMI_BIN_DIR));
    std::cout.flags(std::ios::unitbuf);
    console = new CConsoleHandler();

    const boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
    logConfig = new CBasicLogConfigurator(logPath, console);
    logConfig->configureDefault();

    // XXX: apparently this needs to be invoked before Settings() stuff
    preinitDLL(::console, false);

    boost::filesystem::current_path(wd);
    processArguments(
        stateEncoding,
        map,
        loglevelGlobal,
        loglevelAI,
        loglevelStats,
        redAI,
        blueAI,
        redModel,
        blueModel,
        maxBattles,
        seed,
        randomHeroes,
        randomObstacles,
        swapSides,
        statsMode,
        statsStorage,
        statsPersistFreq,
        statsSampling,
        statsScoreVar,
        printModelPredictions
    );

    // chdir needed for VCMI init
    boost::filesystem::current_path(boost::filesystem::path(VCMI_BIN_DIR));

    // printf("map: %s\n", map.c_str());
    // printf("loglevelGlobal: %s\n", loglevelGlobal.c_str());
    // printf("loglevelAI: %s\n", loglevelAI.c_str());
    // printf("redAI: %s\n", redAI.c_str());
    // printf("blueAI: %s\n", blueAI.c_str());
    // printf("redModel: %s\n", redModel.c_str());
    // printf("blueModel: %s\n", blueModel.c_str());
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
    confcolor("stats",  "trace", "gray");
    confcolor("global", "debug", "gray");
    confcolor("ai",     "debug", "gray");
    confcolor("stats",  "debug", "gray");
    confcolor("global", "info", "white");
    confcolor("ai",     "info", "white");
    confcolor("stats",  "info", "white");
    confcolor("global", "warn", "yellow");
    confcolor("ai",     "warn", "yellow");
    confcolor("stats",  "warn", "yellow");
    confcolor("global", "error", "red");
    confcolor("ai",     "error", "red");
    confcolor("stats",  "error", "red");

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
        CStopWatch tmh;
        try
        {
            loadDLLClasses(true);
            CGI->setFromLib();
        }
        catch (const DataLoadingException & e)
        {
            criticalInitializationError = e.what();
            return;
        }

        logGlobal->info("Initializing VCMI_Lib: %d ms", tmh.getDiff());
    });
    loading.join();

    if (criticalInitializationError.has_value()) {
        auto msg = criticalInitializationError.value();
        logGlobal->error("FATAL ERROR ENCOUTERED, VCMI WILL NOW TERMINATE");
        logGlobal->error("Reason: %s", msg);
        std::string messageToShow = "Fatal error! " + msg;
        throw std::runtime_error(msg);
    }

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

    if(headless) {
        while (true) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        }
    } else {
        GH.screenHandler().clearScreen();
        while(true) {
            GH.input().fetchEvents();
            GH.renderFrame();
        }
    }
}

