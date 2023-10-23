#include <cstdio>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "myclient.h"

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

#include "../lib/filesystem/Filesystem.h"
#include "../lib/CGeneralTextHandler.h"
#include "../lib/VCMIDirs.h"
#include "../lib/VCMI_Lib.h"
#include "../lib/CConfigHandler.h"

namespace bfs = boost::filesystem;

extern boost::thread_specific_ptr<bool> inGuiThread;

static CBasicLogConfigurator *logConfig;

void myinit()
{
  loadDLLClasses();
  const_cast<CGameInfo*>(CGI)->setFromLib();
}

int main(int argc, char * argv[])
{
  boost::filesystem::current_path(boost::filesystem::system_complete(argv[0]).parent_path());
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

  logConfig->configure();
  logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

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

  boost::thread loading(myinit);
  loading.join();

  GH.screenHandler().clearScreen();

  graphics = new Graphics(); // should be before curh
  CCS->curh = new CursorHandler();
  CMessage::init();
  CCS->curh->show();
  std::shared_ptr<CMainMenu> mmenu;
  mmenu = CMainMenu::create();
  GH.curInt = mmenu.get();
  CSH->uuid = "00000000-0000-0000-0000-000000000000";
  std::vector<std::string> names;
  names.push_back("simobot");

  // ESelectionScreen sscreen = ESelectionScreen::loadGame;
  ESelectionScreen sscreen = ESelectionScreen::newGame;
  mmenu->openLobby(sscreen, true, &names, ELoadMode::MULTI);

  inGuiThread.reset(new bool(true));

  while(1) //main SDL events loop
  {
    GH.input().fetchEvents();
    CSH->applyPacksOnLobbyScreen();
    GH.renderFrame();
  }

  return 0;
}

// void handleQuit(bool ask)
// {
//   if(CSH->client && LOCPLINT && ask)
//   {
//     CCS->curh->set(Cursor::Map::POINTER);
//     LOCPLINT->showYesNoDialog(CGI->generaltexth->allTexts[69], quitApplication, nullptr);
//   }
//   else
//   {
//     quitApplication();
//   }
// }
