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

#include "StdInc.h"
#include "AsyncRunner.h"
#include "CMT.h"
#include "AI/MMAI/schema/base.h"

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>
#include <condition_variable>

#include "AI/MMAI/schema/schema.h"
#include "ExceptionsCommon.h"
#include "GameEngine.h"
#include "GameInstance.h"
#include "MLClient.h"

#include "lib/filesystem/Filesystem.h"
#include "lib/texts/CGeneralTextHandler.h"
#include "lib/VCMIDirs.h"
#include "lib/GameLibrary.h"
#include "lib/CConfigHandler.h"

#include "lib/logging/CBasicLogConfigurator.h"

#include "client/StdInc.h"
#include "lib/filesystem/Filesystem.h"
#include "lib/logging/CBasicLogConfigurator.h"
#include "lib/CConsoleHandler.h"
#include "lib/VCMIDirs.h"
#include "mainmenu/CMainMenu.h"
#include "media/CEmptyVideoPlayer.h"
#include "media/CMusicHandler.h"
#include "media/CSoundHandler.h"
#include "media/CVideoHandler.h"
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
#include "lib/GameLibrary.h"
#include "lib/CConfigHandler.h"
#include "render/IRenderHandler.h"
#include "vstd/CLoggerBase.h"
#include "windows/InfoWindows.h"

static std::optional<std::string> criticalInitializationError;
std::atomic<bool> headlessQuit = false;
CBasicLogConfigurator *logConfig;
CConsoleHandler *console;

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

[[noreturn]] static void quitApplicationImmediately(int error_code)
{
    // Perform quick exit without executing static destructors and let OS cleanup anything that we did not
    // We generally don't care about them and this leads to numerous issues, e.g.
    // destruction of locked mutexes (fails an assertion), even in third-party libraries (as well as native libs on Android)
    // Android - std::quick_exit is available only starting from API level 21
    // Mingw, macOS and iOS - std::quick_exit is unavailable (at least in current version of CI)
#if (defined(__ANDROID_API__) && __ANDROID_API__ < 21) || (defined(__MINGW32__)) || defined(VCMI_APPLE)
    ::exit(error_code);
#else
    std::quick_exit(error_code);
#endif
}


/// Notify user about encountered fatal error and terminate the game
/// TODO: decide on better location for this method
void handleFatalError(const std::string & message, bool terminate)
{
    logGlobal->error("FATAL ERROR ENCOUNTERED, VCMI WILL NOW TERMINATE");
    logGlobal->error("Reason: %s", message);

    std::string messageToShow = "Fatal error! " + message;

    // SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error!", messageToShow.c_str(), nullptr);

    if (terminate)
        throw std::runtime_error(message);
    else
        ::exit(1);
}

namespace ML {
    void shutdown_vcmi() {
        auto l = std::lock_guard(mutex_shutdown);
        cond_shutdown.notify_all();
        flag_shutdown = true;

        GAME->server().endNetwork();

        if(!settings["session"]["headless"].Bool())
        {
            if(GAME->server().client)
                GAME->server().endGameplay();

            if (ENGINE)
                ENGINE->windows().clear();
        }

        GAME.reset();
        // must be executed before reset - since unique_ptr resets pointer to null before calling destructor
        ENGINE->async().wait();
        ENGINE.reset();
        delete LIBRARY;
        LIBRARY = nullptr;
        logConfig->deconfigure();
        delete logConfig;

        std::cout << "Ending...\n";
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

        if (a.randomStackChance < 0 || a.randomStackChance > 100) {
            std::cerr << "Bad value for randomStackChance: expected an integer between 0 and 100, got: " << a.randomStackChance << "\n";
            exit(1);
        }

        if (a.tightFormationChance < 0 || a.tightFormationChance > 100) {
            std::cerr << "Bad value for tightFormationChance: expected an integer between 0 and 100, got: " << a.tightFormationChance << "\n";
            exit(1);
        }

        if (a.randomTerrainChance < 0 || a.randomTerrainChance > 100) {
            std::cerr << "Bad value for randomTerrainChance: expected an integer between 0 and 100, got: " << a.randomTerrainChance << "\n";
            exit(1);
        }

        if (a.leftVipChance < 0 || a.leftVipChance > 100) {
            std::cerr << "Bad value for leftVipChance: expected an integer between 0 and 100, got: " << a.leftVipChance << "\n";
            exit(1);
        }

        if (a.rightVipChance < 0 || a.rightVipChance > 100) {
            std::cerr << "Bad value for rightVipChance: expected an integer between 0 and 100, got: " << a.rightVipChance << "\n";
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
        auto dir = VCMIDirs::get().userDataPath() / "Maps";
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

        // Prevent misconfigured paths at boot during ML training
        for (auto &model : {a.leftModel, a.rightModel}) {
            if (model->getType() != MMAI::Schema::ModelType::PATH)
                continue;

            auto rpath = ResourcePath(model->getName());
            auto loaders = CResourceHandler::get()->getResourcesWithName(rpath);

            if (loaders.size() != 1) {
                std::cerr << "Bad torch model path: " << model->getName() << "\nMust be a valid VCMI filesystem path, e.g. MMAI/models/defender-v5.pt";
                exit(1);
            }
        }
    }

    void processArguments(InitArgs &a) {
        headless = a.headless;
        baggage = new MMAI::Schema::Baggage;
        baggage->modelLeft = a.leftModel;
        baggage->modelRight = a.rightModel;
        baggage->allowMlBotLeft = a.leftAllowMlBot;
        baggage->allowMlBotRight = a.rightAllowMlBot;

        Settings(settings.write({"adventure", "quickCombat"}))->Bool() = headless;
        Settings(settings.write({"session", "headless"}))->Bool() = headless;
        Settings(settings.write({"session", "onlyai"}))->Bool() = headless;
        Settings(settings.write({"session", "disableVideo"}))->Bool() = true;
        Settings(settings.write({"server", "localPort"}))->Integer() = 0;
        Settings(settings.write({"server", "useProcess"}))->Bool() = false;
        Settings(settings.write({"server", "seed"}))->Integer() = a.seed;
        // Re-use seed from global server config
        // (the ML server plugin uses a different RNG)
        Settings(settings.write({"server", "ML", "seed"}))->Integer() = a.seed;
        Settings(settings.write({"server", "ML", "maxBattles"}))->Integer() = a.maxBattles;
        Settings(settings.write({"server", "ML", "randomHeroes"}))->Integer() = a.randomHeroes;
        Settings(settings.write({"server", "ML", "randomObstacles"}))->Integer() = a.randomObstacles;
        Settings(settings.write({"server", "ML", "townChance"}))->Integer() = a.townChance;
        Settings(settings.write({"server", "ML", "warmachineChance"}))->Integer() = a.warmachineChance;
        Settings(settings.write({"server", "ML", "randomStackChance"}))->Integer() = a.randomStackChance;
        Settings(settings.write({"server", "ML", "tightFormationChance"}))->Integer() = a.tightFormationChance;
        Settings(settings.write({"server", "ML", "randomTerrainChance"}))->Integer() = a.randomTerrainChance;
        Settings(settings.write({"server", "ML", "leftVipChance"}))->Integer() = a.leftVipChance;
        Settings(settings.write({"server", "ML", "rightVipChance"}))->Integer() = a.rightVipChance;
        Settings(settings.write({"server", "ML", "battlefieldPattern"}))->String() = a.battlefieldPattern;
        Settings(settings.write({"server", "ML", "manaMin"}))->Integer() = a.manaMin;
        Settings(settings.write({"server", "ML", "manaMax"}))->Integer() = a.manaMax;
        Settings(settings.write({"server", "ML", "swapSides"}))->Integer() = a.swapSides;
        Settings(settings.write({"server", "ML", "statsMode"}))->String() = a.statsMode;
        Settings(settings.write({"server", "ML", "statsStorage"}))->String() = a.statsStorage;
        Settings(settings.write({"server", "ML", "statsTimeout"}))->Integer() = a.statsTimeout;
        Settings(settings.write({"server", "ML", "statsPersistFreq"}))->Integer() = a.statsPersistFreq;
        Settings(settings.write({"server", "ML", "statsLoglevel"}))->String() = a.loglevelStats;

        // Adventure MMAI = AAI
        // Combat MMAI = BAI

        // Set all adventure AIs to MMAI (i.e. AAI, which creates BAI for combat)
        Settings(settings.write({"ai", "adventureAlliedAI"}))->String() = "MMAI";
        Settings(settings.write({"ai", "adventureEnemyAI"}))->String() = "MMAI";
        Settings(settings.write({"session", "oneGoodAI"}))->Bool() = false;

        // With GUI, the player's "adventure" AI is CPlayerInterface
        // When auto-combat is pressed, it creates whatever combatAlliedAI says
        Settings(settings.write({"server", "combatAlliedAI"}))->String() = "MMAI";

        // Set all adventure AIs to AAI, which always create BAIs


        // Set max difficulty (affects BattleAI number of simulated turns)
        // TODO: make configurable
        Settings(settings.write({"general", "lastDifficulty"}))->Integer() = 3;

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
        // Store original shell workdir (as VCMI will chdir to VCMI_BIN_DIR)
        // The original workdir is used for loading models specified by relative paths
        // (then is again changed to VCMI_BIN_DIR to prevent VCMI errors)
        auto wd = fs::current_path();

        std::cout << "INITARGS: " << a.leftVipChance << "|" << a.rightVipChance << "\n";

        // chdir needed for VCMI init
        fs::current_path(fs::path(VCMI_BIN_DIR));
        std::cout.flags(std::ios::unitbuf);
        console = new CConsoleHandler();
        console->start();

        const boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
        logConfig = new CBasicLogConfigurator(logPath, console);
        logConfig->configureDefault();

        LIBRARY = new GameLibrary;
        LIBRARY->initializeFilesystem(false);

        // validating after preinitDLL as the VCMIDirs are not initialized before it
        validateArguments(a);
        processArguments(a);

        // printf("map: %s\n", map.c_str());
        // printf("loglevelGlobal: %s\n", loglevelGlobal.c_str());
        // printf("loglevelAI: %s\n", loglevelAI.c_str());
        // printf("headless: %d\n", headless);

        Settings(settings.write({"battle", "speedFactor"}))->Integer() = 5;
        Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
        Settings(settings.write({"battle", "stickyHeroInfoWindows"}))->Bool() = false;
        Settings(settings.write({"logging", "console", "format"}))->String() = "[%t][%n] %l %m";
        Settings(settings.write({"logging", "console", "coloredOutputEnabled"}))->Bool() = true;
        Settings(settings.write({"logging", "console", "threshold"}))->String() = "trace";

        Settings(settings.write({"session", "disableVideo"}))->Bool() = true;
        Settings(settings.write({"video", "fullscreen"}))->Bool() = false;
        Settings(settings.write({"battle", "cellBorders"}))->Bool() = true;
        Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
        Settings(settings.write({"battle", "speedFactor"}))->Integer() = 9;

        // logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

        logConfig->configure();

        // XXX: EntryPoint does NOT init ENGINE if headless, but I don't think it can work without it
        // TODO: add flag in GameEngine to skip sound and music init if headless
        ENGINE = std::make_unique<GameEngine>(headless);

        auto aco = AICombatOptions();
        aco.other = std::make_any<MMAI::Schema::Baggage*>(baggage);
        GAME = std::make_unique<GameInstance>(aco);

        if (!headless)
            ENGINE->setEngineUser(GAME.get());

        boost::thread loading([]() {
            try
            {
                CStopWatch tmh;
                LIBRARY->initializeLibrary();
                logGlobal->info("Initializing VCMI_Lib: %d ms", tmh.getDiff());
            }
            catch (const DataLoadingException & e)
            {
                criticalInitializationError = e.what();
                return;
            }
        });
        loading.join();

        if (criticalInitializationError.has_value())
            handleFatalError(criticalInitializationError.value(), false);

        if (!headless) {
            graphics = new Graphics(); // should be before curh
            ENGINE->renderHandler().onLibraryLoadingFinished(LIBRARY);
            CMessage::init();
            ENGINE->cursor().init();
            ENGINE->cursor().show();
        }
    }

    void start_vcmi() {
        if (mapname == "")
            throw std::runtime_error("call init_vcmi first");

        logGlobal->info("friendlyAI -> " + settings["ai"]["combatAlliedAI"].String());
        logGlobal->info("playerAI -> " + settings["ai"]["adventureEnemyAI"].String());
        logGlobal->info("enemyAI -> " + settings["ai"]["combatEnemyAI"].String());
        logGlobal->info("headless -> " + std::to_string(settings["session"]["headless"].Bool()));
        logGlobal->info("onlyai -> " + std::to_string(settings["session"]["onlyai"].Bool()));
        logGlobal->info("quickCombat -> " + std::to_string(settings["adventure"]["quickCombat"].Bool()));

        auto &si = GAME->server();
        if (headless) {
            auto l = std::unique_lock(mutex_shutdown);
            auto t = boost::thread([&si]() {
                si.debugStartTest(mapname, false);
            });
            cond_shutdown.wait(l);
            std::cout << "VCMI shutdown complete.\n";
            t.join();
        } else {
            // GAME->mainmenu()->makeActiveInterface();
            si.debugStartTest(mapname, false);
            ENGINE->mainLoop();
        }

        shutdown_vcmi();
        quitApplicationImmediately(0);
    }
}
