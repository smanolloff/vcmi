#include <cstdio>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "pyclient.h"
#include "logging/CLogger.h"

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

extern boost::thread_specific_ptr<bool> inGuiThread;

static CBasicLogConfigurator *logConfig;

const MMAI::Export::F_Sys init_vcmi(
  std::string resdir,
  std::string loglevelGlobal,
  std::string loglevelAI,
  MMAI::Export::CBProvider * cbprovider
) {
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
  logGlobal->info("Starting client of '%s'", GameConstants::VCMI_VERSION);
  logGlobal->info("The log file will be saved to %s", logPath);

  preinitDLL(::console);

  Settings(settings.write({"session", "headless"}))->Bool() = true;
  Settings(settings.write({"session", "onlyai"}))->Bool() = true;
  Settings(settings.write({"server", "playerAI"}))->String() = "MMAI";
  // NOTE: friendlyAI is hard-coded in MMAI's AAI::getBattleAIName()
  Settings(settings.write({"server", "neutralAI"}))->String() = "StupidAI";
  // Settings(settings.write({"logging", "console", "format"}))->String() = "[VCMI] %c [%n] %l %m";
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

  // this line must come after "conflog" stuff
  logConfig->configure();

  srand ( (unsigned int)time(nullptr) );

  // This initializes SDL and requires main thread.
  // GH.init();

  CCS = new CClientState();
  CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
  auto baggage = std::make_any<MMAI::Export::CBProvider*>(cbprovider);
  CSH = new CServerHandler(baggage);

  boost::thread loading([]() {
    loadDLLClasses();
    const_cast<CGameInfo*>(CGI)->setFromLib();
  });
  loading.join();

  // graphics = new Graphics(); // should be before curh
  // CCS->curh = new CursorHandler();
  // CMessage::init();
  // CCS->curh->show();

  // We have the GIL, safe to call syscbcb now
  return [](std::string cmd) {
    logGlobal->error("!!!!!!!!!!! SYS !!!!!!!!!! Received command: %s", cmd);
    if (cmd == "terminate")
      exit(0);
    else if (cmd == "reset")
      CSH->sendRestartGame();
    else
      logGlobal->error("Unknown sys command: '%s'", cmd);
  };
}

void start_vcmi(std::string mapname) {
  auto t = boost::thread(&CServerHandler::debugStartTest, CSH, std::string("Maps/") + mapname, false);
  inGuiThread.reset(new bool(true));
  // GH.screenHandler().close();

  while (true) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
  }
}
