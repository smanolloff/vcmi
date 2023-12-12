#include <cstdio>
#include <iostream>
#include <dlfcn.h>

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

void myinit()
{
    loadDLLClasses();
    const_cast<CGameInfo*>(CGI)->setFromLib();
}


static void mainLoop()
{
    inGuiThread.reset(new bool(true));

    while(1) //main SDL events loop
    {
        GH.input().fetchEvents();
        CSH->applyPacksOnLobbyScreen();
        GH.renderFrame();
    }
}

// build/bin/myclient
// /Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin
// int mymain(std::string resourcedir, bool headless, const std::function<void(int)> &callback) {
int mymain(
    MMAI::Export::Baggage* baggage,
    std::string resdir,
    std::string mapname,
    std::string loglevelGlobal,
    std::string loglevelAI,
    std::string attackerAI,
    std::string defenderAI,
    std::string attackerModel,
    std::string defenderModel
) {
    if (!baggage) throw std::runtime_error("baggage is required");
    if (!baggage->f_getAction) throw std::runtime_error("baggage->f_getAction is required");

    // Default f_idGetAction is a simple proxy to f_getAction
    baggage->f_idGetAction = [&baggage](MMAI::Export::Side _, const MMAI::Export::Result* result) {
        printf("*** BASE WRAPPER: CALL USER FN ***");
        return baggage->f_getAction(result);
    };

    // the CWD will change for VCMI to work in non-dist mode
    boost::filesystem::path oldcwd = boost::filesystem::current_path();
    boost::filesystem::current_path(boost::filesystem::path(resdir));
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

    // XXX: apparently this needs to be invoked before Settings() changes
    preinitDLL(::console);

    //
    // NOTE:
    // This requires 2-player maps where player 1 is HUMAN
    // The player hero moves 1 tile to the right and engages into a battle
    //
    // XXX:
    // This will not work unless the current dir is vcmi-gym's root dir
    // (the python module resolution gets messed up otherwise)
    auto loadmodel = [&oldcwd](MMAI::Export::Side side, std::string model) {
        boost::filesystem::path savecwd = boost::filesystem::current_path();
        boost::filesystem::current_path(oldcwd);

        //
        // XXX: this makes it impossible to use lldb (invalid instruction error...)
        //
        auto libfile = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/connector/build/libloader.dylib";
        void* handle = dlopen(libfile, RTLD_LAZY);
        if(!handle) throw std::runtime_error("Error loading the library: " + std::string(dlerror()));

        auto init = reinterpret_cast<decltype(&ConnectorLoader_init)>(dlsym(handle, "ConnectorLoader_init"));
        if(!init) throw std::runtime_error("Error getting init fn: " + std::string(dlerror()));

        auto idGetAction = reinterpret_cast<decltype(&ConnectorLoader_getAction)>(dlsym(handle, "ConnectorLoader_getAction"));
        if(!idGetAction) throw std::runtime_error("Error getting idGetAction fn: " + std::string(dlerror()));

        // preemptive init done in myclient to avoid freezing at first click of "auto-combat"
        init(side, model);
        logGlobal->error("INIT AI DONE FOR: " + std::to_string(static_cast<int>(side)));

        boost::filesystem::current_path(savecwd);
        return idGetAction;
    };


    // Notes on AI creation
    //
    // *** Game start - adventure interfaces ***
    // initGameInterface() is called on:
    // * CPlayerInterface (CPI) for humans
    // * settings["playerAI"] for computers
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
    // Attacker is human, ie. CPI
    // It creates battle interfaces as per settings["friendlyAI"]
    // (CPI is modded to pass the baggage, which is used only by MMAI::BAI)
    //
    if (attackerAI == AI_MMAI_MODEL) {
        Settings(settings.write({"server", "friendlyAI"}))->String() = "MMAI";

        // if not static => SIGSEGV...
        static auto idGetActionAttacker = loadmodel(MMAI::Export::Side::ATTACKER, attackerModel);
        static auto fallbackAttacker = baggage->f_idGetAction;
        baggage->f_idGetAction = [](MMAI::Export::Side side, const MMAI::Export::Result* result) {
            return (side == MMAI::Export::Side::ATTACKER)
                ? idGetActionAttacker(side, result)
                : fallbackAttacker(side, result);
        };
    } else if (attackerAI == AI_MMAI_USER) {
        Settings(settings.write({"server", "friendlyAI"}))->String() = "MMAI";
    } else if (attackerAI == AI_STUPIDAI) {
        Settings(settings.write({"server", "friendlyAI"}))->String() = "StupidAI";
    } else if (attackerAI == AI_BATTLEAI) {
        Settings(settings.write({"server", "friendlyAI"}))->String() = "BattleAI";
    } else {
        throw std::runtime_error("Unexpected attackerAI: " + attackerAI);
    }

    //
    // AI for defender (aka. computer, aka. some AI)
    // Defender is computer with adventure interface from settings["playerAI"]:
    //
    // * "VCAI", which will create battle interfaces as per settings["enemyAI"]
    //   (will not pass baggage if settings["enemyAI"]="MMAI")
    //
    // * "MMAI", which will always create MMAI::BAI battle interfaces
    //   (will pass baggage)
    //
    if (defenderAI == AI_MMAI_MODEL) {
        // baggage needed => use MMAI adv. interface
        Settings(settings.write({"server", "playerAI"}))->String() = "MMAI";

        // if not static => SIGSEGV...
        static auto idGetActionDefender = loadmodel(MMAI::Export::Side::DEFENDER, defenderModel);
        static auto fallbackDefender = baggage->f_idGetAction;

        baggage->f_idGetAction = [](MMAI::Export::Side side, const MMAI::Export::Result* result) {
            return (side == MMAI::Export::Side::DEFENDER)
                ? idGetActionDefender(side, result)
                : fallbackDefender(side, result);
        };
    } else if (defenderAI == AI_MMAI_USER) {
        // baggage needed => use MMAI adv. interface
        Settings(settings.write({"server", "playerAI"}))->String() = "MMAI";
    } else if (defenderAI == AI_STUPIDAI) {
        Settings(settings.write({"server", "playerAI"}))->String() = "VCAI";
        Settings(settings.write({"server", "enemyAI"}))->String() = "StupidAI";
    } else if (defenderAI == AI_BATTLEAI) {
        Settings(settings.write({"server", "playerAI"}))->String() = "VCAI";
        Settings(settings.write({"server", "enemyAI"}))->String() = "BattleAI";
    } else {
        throw std::runtime_error("Unexpected defenderAI: " + defenderAI);
    }

    Settings(settings.write({"session", "headless"}))->Bool() = false;
    Settings(settings.write({"session", "onlyai"}))->Bool() = false;
    Settings(settings.write({"adventure", "quickCombat"}))->Bool() = false;
    Settings(settings.write({"battle", "speedFactor"}))->Integer() = 5;
    Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
    Settings(settings.write({"battle", "stickyHeroInfoWindows"}))->Bool() = false;

    // convert to "ai/simotest.vmap" to "maps/ai/simotest.vmap"
    auto mappath = std::filesystem::path("maps") / std::filesystem::path(mapname);
    // convert to "maps/ai/simotest.vmap" to "maps/ai/simotest"
    auto mappathstr = (mappath.parent_path() / mappath.stem()).string();
    // convert to "maps/ai/simotest" to "MAPS/AI/SIMOTEST"
    std::transform(mappathstr.begin(), mappathstr.end(), mappathstr.begin(), [](unsigned char c) { return std::toupper(c); });
    // Set "lastMap" to prevent some race condition debugStartTest+Menu screen
    Settings(settings.write({"general", "lastMap"}))->String() = mappathstr;

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

    logConfig->configure();
    // logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

    // Some basic data validation to produce better error messages in cases of incorrect install
    auto testFile = [](std::string filename, std::string message)
    {
        if (!CResourceHandler::get()->existsResource(ResourceID(filename)))
            handleFatalError(message, false);
    };

    testFile("DATA/HELP.TXT", "VCMI requires Heroes III: Shadow of Death or Heroes III: Complete data files to run!");
    testFile("MODS/VCMI/MOD.JSON", "VCMI installation is corrupted! Built-in mod was not found!");
    testFile("DATA/PLAYERS.PAL", "Heroes III data files are missing or corruped! Please reinstall them.");
    testFile("SPRITES/DEFAULT.DEF", "Heroes III data files are missing or corruped! Please reinstall them.");
    testFile("DATA/TENTCOLR.TXT", "Heroes III: Restoration of Erathia (including HD Edition) data files are not supported!");

    srand ( (unsigned int)time(nullptr) );

    GH.init();

    CCS = new CClientState();
    CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
    CSH = new CServerHandler(std::make_any<MMAI::Export::Baggage*>(baggage));

    CSH->uuid = "00000000-0000-0000-0000-000000000000";

    CCS->videoh = new CEmptyVideoPlayer();

    CCS->soundh = new CSoundHandler();
    CCS->soundh->init();
    CCS->soundh->setVolume((ui32)settings["general"]["sound"].Float());
    CCS->musich = new CMusicHandler();
    CCS->musich->init();
    CCS->musich->setVolume((ui32)settings["general"]["music"].Float());

    boost::thread loading([]() {
        loadDLLClasses();
        const_cast<CGameInfo*>(CGI)->setFromLib();
    });
    loading.join();

    graphics = new Graphics(); // should be before curh
    CCS->curh = new CursorHandler();
    CMessage::init();
    CCS->curh->show();

    boost::thread t(&CServerHandler::debugStartTest, CSH, std::string("Maps/") + mapname, false);
    inGuiThread.reset(new bool(true));


    if(settings["session"]["headless"].Bool()) {
        GH.screenHandler().close();
        while (true) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(7000));
        }
    } else {
        GH.screenHandler().clearScreen();
        // mainLoop cant be in another thread -- SDL can render in main thread only
        // boost::thread([]() { mainLoop(); });
        mainLoop();
    }

    return 0;
}
