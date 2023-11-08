#include <cstdio>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "logging/CLogger.h"
#include "pyclient.h"

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

#include <string_view>

namespace bfs = boost::filesystem;

extern boost::thread_specific_ptr<bool> inGuiThread;

static CBasicLogConfigurator *logConfig;

void preinit_vcmi(std::string resdir) {
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

  const bfs::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
  logConfig = new CBasicLogConfigurator(logPath, console);
  logConfig->configureDefault();
  logGlobal->info("Starting client of '%s'", GameConstants::VCMI_VERSION);
  logGlobal->info("The log file will be saved to %s", logPath);

  preinitDLL(::console);

  Settings(settings.write({"session", "headless"}))->Bool() = true;
  Settings(settings.write({"session", "onlyai"}))->Bool() = true;
  Settings(settings.write({"server", "playerAI"}))->String() = "MyAI";
  // NOTE: friendlyAI is hard-coded in MyAI's AAI::getBattleAIName()
  Settings(settings.write({"server", "neutralAI"}))->String() = "StupidAI";
  Settings(settings.write({"logging", "console", "format"}))->String() = "[%t][%n] %l %m";

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

  conflog("global", "error");
  conflog("ai", "debug");

  logConfig->configure();

  srand ( (unsigned int)time(nullptr) );

  // This initializes SDL and requires main thread.
  GH.init();
}

void start_vcmi(std::string mapname, MMAI::CBProvider cbprovider) {
  CCS = new CClientState();
  CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
  auto baggage = std::make_any<MMAI::CBProvider*>(&cbprovider);
  CSH = new CServerHandler(baggage);

  boost::thread loading([]() {
    loadDLLClasses();
    const_cast<CGameInfo*>(CGI)->setFromLib();
  });
  loading.join();

  graphics = new Graphics(); // should be before curh
  CCS->curh = new CursorHandler();
  CMessage::init();
  CCS->curh->show();

  // We have the GIL, safe to call syscbcb now
  cbprovider.syscbcb([](std::string cmd) {
    logGlobal->error("!!!!!!!!!!! SYS !!!!!!!!!! Received command: %s", cmd);
    if (cmd == "terminate")
      exit(0);
    else if (cmd == "reset")
      CSH->sendRestartGame();
    else
      logGlobal->error("Unknown sys command: '%s'", cmd);
  });

  auto t = boost::thread(&CServerHandler::debugStartTest, CSH, std::string("Maps/") + mapname, false);
  inGuiThread.reset(new bool(true));
  GH.screenHandler().close();

  while (true) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
  }
}
