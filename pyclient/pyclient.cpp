#include <cstdio>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

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

#include <string_view>

namespace bfs = boost::filesystem;

extern boost::thread_specific_ptr<bool> inGuiThread;

static CBasicLogConfigurator *logConfig;

int start_vcmi(
  std::string resdir,
  std::string mapname,
  CBProvider cbprovider,
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

  const bfs::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
  logConfig = new CBasicLogConfigurator(logPath, console);
  logConfig->configureDefault();
  logGlobal->info("Starting client of '%s'", GameConstants::VCMI_VERSION);
  logGlobal->info("The log file will be saved to %s", logPath);

  preinitDLL(::console);

  Settings(settings.write({"session", "headless"}))->Bool() = true;
  Settings(settings.write({"session", "onlyai"}))->Bool() = true;
  Settings(settings.write({"server", "playerAI"}))->String() = "MyAdventureAI";
  Settings(settings.write({"server", "friendlyAI"}))->String() = "StupidAI";
  Settings(settings.write({"server", "neutralAI"}))->String() = "StupidAI";
  Settings(settings.write({"logging", "console", "format"}))->String() = "[%t][%n] %m";

  logConfig->configure();

  srand ( (unsigned int)time(nullptr) );

  GH.init();
  CCS = new CClientState();
  CGI = new CGameInfo(); //contains all global informations about game (texts, lodHandlers, map handler etc.)
  CSH = new CServerHandler(std::make_any<CBProvider*>(&cbprovider));

  boost::thread loading([]() {
    loadDLLClasses();
    const_cast<CGameInfo*>(CGI)->setFromLib();
  });
  loading.join();

  graphics = new Graphics(); // should be before curh
  CCS->curh = new CursorHandler();
  CMessage::init();
  CCS->curh->show();

  // We have the GIL, safe to call pycbsysinit now
  cbprovider.pycbsysinit([CSH](std::string cmd) {
    std::string_view svcmd(cmd);

    switch (svcmd) {
      case "terminate"sv:
        exit(0);
      case "reset"sv:
        CSH->sendRestartGame();
        break;
      default:
        logGlobal->error("Unknown sys command: '%s'", cmd);
        break;
    }
  })

  boost::thread(&CServerHandler::debugStartTest, CSH, std::string("Maps/") + mapname, false, cbprovider);
  inGuiThread.reset(new bool(true));
  GH.screenHandler().close();

  while (true) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
  }

  return 0;
}
