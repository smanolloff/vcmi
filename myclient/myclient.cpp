#include <cstdio>
#include <iostream>
#include <dlfcn.h>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

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
int mymain(std::string resdir, std::string mapname, bool ai) {
  //
  // Init AI stuff
  // XXX: this makes it impossible to use lldb (invalid instruction error)
  // comment out all this code and use dummy getAction below instead:
  // auto getAction = [](const MMAI::Export::Result* r) { return 42; };
  //
  auto libfile = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/connector/build/libloader.dylib";
  void* handle = dlopen(libfile, RTLD_LAZY);
  if(!handle) throw std::runtime_error("Error loading the library: " + std::string(dlerror()));

  auto init = reinterpret_cast<decltype(&ConnectorLoader_init)>(dlsym(handle, "ConnectorLoader_init"));
  if(!init) throw std::runtime_error("Error getting init fn: " + std::string(dlerror()));

  auto getAction = reinterpret_cast<decltype(&ConnectorLoader_getAction)>(dlsym(handle, "ConnectorLoader_getAction"));
  if(!getAction) throw std::runtime_error("Error getting getAction fn: " + std::string(dlerror()));

  // preemptive init done in myclient to avoid freezing at first click of "auto-combat"
  init();
  logGlobal->error("INIT AI DONE");

  //
  // EOF Init AI stuff
  //

  // rest

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

  preinitDLL(::console);

  Settings(settings.write({"session", "headless"}))->Bool() = false;
  Settings(settings.write({"session", "onlyai"}))->Bool() = false;
  Settings(settings.write({"server", "playerAI"}))->String() = "VCAI";
  Settings(settings.write({"server", "friendlyAI"}))->String() = "MMAI";
  Settings(settings.write({"server", "neutralAI"}))->String() = "StupidAI";
  Settings(settings.write({"adventure", "quickCombat"}))->Bool() = false;
  Settings(settings.write({"battle", "speedFactor"}))->Integer() = 5;

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

  conflog("global", "debug");
  conflog("ai", "debug");

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
  CSH = new CServerHandler();

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
