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

#include "CMT.h"
#include "Global.h"
#include "AI/MMAI/schema/base.h"
#include "AI/MMAI/schema/v1/constants.h"
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdio>
#include <iostream>
#include <dlfcn.h>
#include <filesystem>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>

#include "AI/MMAI/schema/schema.h"
#include "ExceptionsCommon.h"
#include "MLClient.h"

#include "lib/filesystem/Filesystem.h"
#include "lib/texts/CGeneralTextHandler.h"
#include "lib/VCMIDirs.h"
#include "lib/VCMI_Lib.h"
#include "lib/CConfigHandler.h"

#include "lib/logging/CBasicLogConfigurator.h"

#include "client/StdInc.h"
#include "client/CGameInfo.h"
#include "lib/filesystem/Filesystem.h"
#include "lib/logging/CBasicLogConfigurator.h"
#include "lib/CConsoleHandler.h"
#include "lib/VCMIDirs.h"
#include "mainmenu/CMainMenu.h"
#include "media/CEmptyVideoPlayer.h"
#include "media/CMusicHandler.h"
#include "media/CSoundHandler.h"
#include "media/CVideoHandler.h"
#include "client/gui/CGuiHandler.h"
#include "client/windows/CMessage.h"
#include "client/CServerHandler.h"
#include "client/ClientCommandManager.h"
#include "client/gui/CursorHandler.h"
#include "client/eventsSDL/InputHandler.h"
#include "client/render/Graphics.h"
#include "client/render/IScreenHandler.h"
#include "client/CPlayerInterface.h"
#include "client/gui/WindowHandler.h"

#include "lib/filesystem/Filesystem.h"
#include "lib/texts/CGeneralTextHandler.h"
#include "lib/VCMIDirs.h"
#include "lib/VCMI_Lib.h"
#include "lib/CConfigHandler.h"
#include "render/IRenderHandler.h"
#include "vstd/CLoggerBase.h"

static std::optional<std::string> criticalInitializationError;
std::atomic<bool> headlessQuit = false;
CBasicLogConfigurator *logConfig;

bool headless;
std::mutex mutex_shutdown;
std::condition_variable cond_shutdown;
bool flag_shutdown = false;

std::string mapname;
MMAI::Schema::Baggage * baggage;

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

namespace ML {
    void shutdown_vcmi() {
        auto l = std::lock_guard(mutex_shutdown);
        cond_shutdown.notify_all();
        flag_shutdown = true;

        CSH->endNetwork();

        if(!settings["session"]["headless"].Bool())
        {
            if(CSH->client)
                CSH->endGameplay();

            GH.windows().clear();
        }

        vstd::clear_pointer(CSH);

        CMM.reset();

        if(!settings["session"]["headless"].Bool())
        {
            // cleanup, mostly to remove false leaks from analyzer
            if(CCS)
            {
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

        // std::cout << "Ending...\n";
        // quitApplicationImmediately(0);
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

    void validateArguments(InitArgs &a) {
        auto wd = boost::filesystem::current_path();

        if (a.statsMode != "disabled" && a.statsMode != "red" && a.statsMode != "blue") {
            std::cerr << "Bad value for statsMode: expected disabled|red|blue, got: " << a.statsMode << "\n";
            exit(1);
        }

        if (a.statsStorage != "-") {
            auto f = std::filesystem::path(a.statsStorage);
            if (!std::filesystem::is_regular_file(f)) {
                std::cerr << "Bad value for statsStorage: file does not exist: " << f << " (hint: use the SQLs in server/ML/sql to create it)\n";
                exit(1);
            }
        }

        if (a.maxBattles < 0) {
            std::cerr << "Bad value for maxBattles: expected a non-negative integer, got: " << a.maxBattles << "\n";
            exit(1);
        }

        if (a.randomHeroes < 0) {
            std::cerr << "Bad value for randomHeroes: expected a non-negative integer, got: " << a.randomHeroes << "\n";
            exit(1);
        }

        if (a.randomObstacles < 0) {
            std::cerr << "Bad value for randomObstacles: expected a non-negative integer, got: " << a.randomObstacles << "\n";
            exit(1);
        }

        if (a.townChance < 0 || a.townChance > 100) {
            std::cerr << "Bad value for townChance: expected an integer between 0 and 100, got: " << a.townChance << "\n";
            exit(1);
        }

        if (a.manaMin < 0 || a.manaMin > 500) {
            std::cerr << "Bad value for manaMin: expected an integer between 0 and 500, got: " << a.manaMin << "\n";
            exit(1);
        }

        if (a.manaMax < a.manaMin || a.manaMax > 500) {
            std::cerr << "Bad value for manaMax: expected an integer between " << a.manaMin << " and 500, got: " << a.manaMax << "\n";
            exit(1);
        }

        if (a.warmachineChance < 0 || a.warmachineChance > 100) {
            std::cerr << "Bad value for warmachineChance: expected an integer between 0 and 100, got: " << a.warmachineChance << "\n";
            exit(1);
        }

        if (a.swapSides < 0) {
            std::cerr << "Bad value for swapSides: expected a non-negative integer, got: " << a.swapSides << "\n";
            exit(1);
        }

        if (a.statsTimeout < 0) {
            std::cerr << "Bad value for statsTimeout: expected a non-negative integer, got: " << a.statsTimeout << "\n";
            exit(1);
        }

        if (a.statsPersistFreq < 0) {
            std::cerr << "Bad value for statsPersistFreq: expected a non-negative integer, got: " << a.statsPersistFreq << "\n";
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

        // XXX: can this blow given preinitDLL is not yet called here?
        validateFile("map", a.mapname, VCMIDirs::get().userDataPath() / "Maps");

        if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), a.loglevelAI) == LOGLEVELS.end()) {
            std::cerr << "Bad value for loglevelAI: " << a.loglevelAI << "\n";
            exit(1);
        }

        if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), a.loglevelGlobal) == LOGLEVELS.end()) {
            std::cerr << "Bad value for loglevelGlobal: " << a.loglevelGlobal << "\n";
            exit(1);
        }

        if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), a.loglevelStats) == LOGLEVELS.end()) {
            std::cerr << "Bad value for loglevelStats: " << a.loglevelStats << "\n";
            exit(1);
        }
    }

    void processArguments(InitArgs &a) {
        headless = a.headless;
        baggage = new MMAI::Schema::Baggage;
        baggage->devMode = true;
        baggage->modelLeft = a.leftModel;
        baggage->modelRight = a.rightModel;

        Settings(settings.write({"adventure", "quickCombat"}))->Bool() = headless;
        Settings(settings.write({"session", "headless"}))->Bool() = headless;
        Settings(settings.write({"session", "onlyai"}))->Bool() = headless;
        Settings(settings.write({"server", "seed"}))->Integer() = a.seed;
        Settings(settings.write({"server", "localPort"}))->Integer() = 0;
        Settings(settings.write({"server", "useProcess"}))->Bool() = false;
        Settings(settings.write({"server", "ML", "maxBattles"}))->Integer() = a.maxBattles;
        Settings(settings.write({"server", "ML", "randomHeroes"}))->Integer() = a.randomHeroes;
        Settings(settings.write({"server", "ML", "randomObstacles"}))->Integer() = a.randomObstacles;
        Settings(settings.write({"server", "ML", "townChance"}))->Integer() = a.townChance;
        Settings(settings.write({"server", "ML", "warmachineChance"}))->Integer() = a.warmachineChance;
        Settings(settings.write({"server", "ML", "manaMin"}))->Integer() = a.manaMin;
        Settings(settings.write({"server", "ML", "manaMax"}))->Integer() = a.manaMax;
        Settings(settings.write({"server", "ML", "swapSides"}))->Integer() = a.swapSides;
        Settings(settings.write({"server", "ML", "statsMode"}))->String() = a.statsMode;
        Settings(settings.write({"server", "ML", "statsStorage"}))->String() = a.statsStorage;
        Settings(settings.write({"server", "ML", "statsTimeout"}))->Integer() = a.statsTimeout;
        Settings(settings.write({"server", "ML", "statsPersistFreq"}))->Integer() = a.statsPersistFreq;
        Settings(settings.write({"server", "ML", "statsLoglevel"}))->String() = a.loglevelStats;

        // Set all adventure AIs to AAI, which always create BAIs
        Settings(settings.write({"server", "playerAI"}))->String() = "MMAI";
        Settings(settings.write({"server", "oneGoodAI"}))->Bool() = false;

        // Set CPlayerInterface (aka. GUI) to create BAI for auto-combat
        Settings(settings.write({"server", "friendlyAI"}))->String() = "MMAI";

        // convert to "ai/mymap.vmap" to "maps/ai/mymap.vmap"
        auto mappath = std::filesystem::path("Maps") / std::filesystem::path(a.mapname);
        // store "maps/ai/mymap.vmap" into global var
        mapname = mappath.string();

        // Set "lastMap" to prevent some race condition debugStartTest+Menu screen
        // convert to "maps/ai/mymap.vmap" to "maps/ai/mymap"
        auto lastmap = (mappath.parent_path() / mappath.stem()).string();
        // convert to "maps/ai/mymap" to "MAPS/AI/MYMAP"
        std::transform(lastmap.begin(), lastmap.end(), lastmap.begin(), [](unsigned char c) { return std::toupper(c); });
        Settings(settings.write({"general", "lastMap"}))->String() = lastmap;

        //
        // Configure logging
        //
        auto getloglevel = [](std::string domain){
            for (auto logger : settings["logging"]["loggers"].Vector())
                if (logger["domain"].String() == domain)
                    return logger["level"].String();

            return std::string("warn");
        };

        auto loglevelRng = getloglevel("rng");
        auto loglevelNetwork = getloglevel("network");
        auto loglevelMod = getloglevel("mod");
        auto loglevelAnimation = getloglevel("animation");
        auto loglevelBonus = getloglevel("bonus");

        // I could not find a way to edit a specific logger's level from
        // within the code
        // => clear and re-add all loggers
        Settings loggers = settings.write["logging"]["loggers"];
        loggers->Vector().clear();

        auto conflog = [&loggers](std::string domain, std::string lvl) {
            JsonNode jlog, jlvl, jdomain;
            jdomain.String() = domain;
            jlvl.String() = lvl;
            jlog.Struct() = std::map<std::string, JsonNode>{{"level", jlvl}, {"domain", jdomain}};
            loggers->Vector().push_back(jlog);
        };

        conflog("global", a.loglevelGlobal);
        conflog("ai", a.loglevelAI);
        conflog("stats", a.loglevelStats);
        conflog("rng", loglevelRng);
        conflog("network", loglevelNetwork);
        conflog("mod", loglevelMod);
        conflog("animation", loglevelAnimation);
        conflog("bonus", loglevelBonus);
    }

    void init_vcmi(InitArgs &a) {
        validateArguments(a);

        // Store original shell workdir (as VCMI will chdir to VCMI_BIN_DIR)
        // The original workdir is used for loading models specified by relative paths
        // (then is again changed to VCMI_BIN_DIR to prevent VCMI errors)
        auto wd = fs::current_path();

        // chdir needed for VCMI init
        fs::current_path(fs::path(VCMI_BIN_DIR));
        std::cout.flags(std::ios::unitbuf);
        console = new CConsoleHandler();

        const boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
        logConfig = new CBasicLogConfigurator(logPath, console);
        logConfig->configureDefault();

        // XXX: apparently this needs to be invoked before Settings() stuff
        preinitDLL(::console, false);

        fs::current_path(wd);

        processArguments(a);

        // chdir needed for VCMI init
        fs::current_path(fs::path(VCMI_BIN_DIR));

        // printf("map: %s\n", map.c_str());
        // printf("loglevelGlobal: %s\n", loglevelGlobal.c_str());
        // printf("loglevelAI: %s\n", loglevelAI.c_str());
        // printf("headless: %d\n", headless);

        Settings(settings.write({"battle", "speedFactor"}))->Integer() = 5;
        Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
        Settings(settings.write({"battle", "stickyHeroInfoWindows"}))->Bool() = false;
        Settings(settings.write({"logging", "console", "format"}))->String() = "[%t][%n] %l %m";
        Settings(settings.write({"logging", "console", "coloredOutputEnabled"}))->Bool() = true;

        logConfig->configure();
        // logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

        srand ( (unsigned int)time(nullptr) );

        if (!headless)
            GH.init();

        CCS = new CClientState();
        CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)

        auto aco = AICombatOptions();
        aco.other = std::make_any<MMAI::Schema::Baggage*>(baggage);
        CSH = new CServerHandler(aco);

        if (!headless) {
            CCS->videoh = new CEmptyVideoPlayer();
            CCS->soundh = new CSoundHandler();
            CCS->soundh->setVolume((ui32)settings["general"]["sound"].Float());
            CCS->musich = new CMusicHandler();
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
            GH.renderHandler().onLibraryLoadingFinished(CGI);
            CCS->curh = new CursorHandler();
            CMessage::init();
            CCS->curh->show();
        }
    }

    void start_vcmi() {
        auto l = std::unique_lock(mutex_shutdown);
        if (mapname == "")
            throw std::runtime_error("call init_vcmi first");

        logGlobal->info("friendlyAI -> " + settings["server"]["friendlyAI"].String());
        logGlobal->info("playerAI -> " + settings["server"]["playerAI"].String());
        logGlobal->info("enemyAI -> " + settings["server"]["enemyAI"].String());
        logGlobal->info("headless -> " + std::to_string(settings["session"]["headless"].Bool()));
        logGlobal->info("onlyai -> " + std::to_string(settings["session"]["onlyai"].Bool()));
        logGlobal->info("quickCombat -> " + std::to_string(settings["adventure"]["quickCombat"].Bool()));

        auto t = boost::thread(&CServerHandler::debugStartTest, CSH, mapname, false);

        if(headless)
         {
            cond_shutdown.wait(l);
            std::cout << "VCMI shutdown complete.\n";
            t.join();
        } else {
            GH.screenHandler().clearScreen();
            while(!cond_shutdown.wait_for(l, std::chrono::seconds(0), []{ return flag_shutdown; })) {
                GH.input().fetchEvents();
                GH.renderFrame();
            }
        }
    }
}
