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

#include <boost/stacktrace/stacktrace.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <condition_variable>
#include <termios.h>
#include <unistd.h>

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
#include "client/Client.h"
#include "client/ClientCommandManager.h"
#include "lib/CRandomGenerator.h"
#include "lib/battle/BattleInfo.h"
#include "lib/gameState/CGameState.h"
#include "lib/networkPacks/PacksForClientBattle.h"
#include "lib/serializer/CMemorySerializer.h"
#include "server/CGameHandler.h"
#include "client/gui/CursorHandler.h"
#include "client/render/Graphics.h"
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

#include <filesystem>

namespace fs = std::filesystem;

static std::optional<std::string> criticalInitializationError;
std::atomic<bool> headlessQuit = false;
CBasicLogConfigurator *logConfig;
CConsoleHandler *console;

bool headless;
bool hotseat;
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

static void clearPendingTerminalInput()
{
	if (::isatty(STDIN_FILENO))
		::tcflush(STDIN_FILENO, TCIFLUSH);
}

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
		setGameLibrary(nullptr);
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

	void validateArguments(const InitArgs & a)
	{
		auto wd = boost::filesystem::current_path();

		if (a.statsMode != "disabled" && a.statsMode != "red" && a.statsMode != "blue") {
			std::cerr << "Bad value for statsMode: expected disabled|red|blue, got: " << a.statsMode << "\n";
			exit(1);
		}

		if (a.statsStorage != "-") {
			auto f = std::filesystem::path(a.statsStorage);
			validateFile("stats-storage", f, wd);
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

		if (a.randomPrimarySkills < 0) {
			std::cerr << "Bad value for randomPrimarySkills: expected a non-negative integer, got: " << a.randomPrimarySkills << "\n";
			exit(1);
		}

		if (a.warmachineChance < 0 || a.warmachineChance > 100) {
			std::cerr << "Bad value for warmachineChance: expected an integer between 0 and 100, got: " << a.warmachineChance << "\n";
			exit(1);
		}

		if (a.randomArmyValueMin < 500 || a.randomArmyValueMin > 10000000) {
			std::cerr << "Bad value for randomArmyValueMin: expected an integer between 500 and 10000000, got: " << a.randomArmyValueMin << "\n";
			exit(1);
		}

		if ((a.randomArmyValueMax < a.randomArmyValueMin) || a.randomArmyValueMax > 10000000) {
			std::cerr << "Bad value for randomArmyValueMax: expected an integer between randomArmyValueMin and 10000000, got: " << a.randomArmyValueMax << "\n";
			exit(1);
		}

		if (a.randomArmyTargetVar < 0 || a.randomArmyTargetVar > 100) {
			std::cerr << "Bad value for randomArmyTargetVar: expected an integer between 0 and 100, got: " << a.randomArmyTargetVar << "\n";
			exit(1);
		}

		if (!std::isfinite(a.leftTargetMod) || a.leftTargetMod <= 0 || std::lround(static_cast<double>(a.randomArmyValueMin) * a.leftTargetMod) < 1 || static_cast<double>(a.randomArmyValueMax) * a.leftTargetMod > std::numeric_limits<int>::max()) {
			std::cerr << "Bad value for leftTargetMod: expected a positive finite multiplier producing a target within integer range, got: " << a.leftTargetMod << "\n";
			exit(1);
		}

		if (!std::isfinite(a.rightTargetMod) || a.rightTargetMod <= 0 || std::lround(static_cast<double>(a.randomArmyValueMin) * a.rightTargetMod) < 1 || static_cast<double>(a.randomArmyValueMax) * a.rightTargetMod > std::numeric_limits<int>::max()) {
			std::cerr << "Bad value for rightTargetMod: expected a positive finite multiplier producing a target within integer range, got: " << a.rightTargetMod << "\n";
			exit(1);
		}

		if (!a.randomArmies && (a.leftTargetMod != 1.0f || a.rightTargetMod != 1.0f)) {
			std::cerr << "Bad target modifier: --left-target-mod and --right-target-mod require --random-armies\n";
			exit(1);
		}

		if (a.leftUniformChance < 0 || a.leftUniformChance > 100) {
			std::cerr << "Bad value for leftUniformChance: expected an integer between 0 and 100, got: " << a.leftUniformChance << "\n";
			exit(1);
		}

		if (a.rightUniformChance < 0 || a.rightUniformChance > 100) {
			std::cerr << "Bad value for rightUniformChance: expected an integer between 0 and 100, got: " << a.rightUniformChance << "\n";
			exit(1);
		}

		if (a.tightFormationChance < 0 || a.tightFormationChance > 100) {
			std::cerr << "Bad value for tightFormationChance: expected an integer between 0 and 100, got: " << a.tightFormationChance << "\n";
			exit(1);
		}

		if (a.creatureBankChance < 0 || a.creatureBankChance > 100) {
			std::cerr << "Bad value for creatureBankChance: expected an integer between 0 and 100, got: " << a.creatureBankChance << "\n";
			exit(1);
		}

		if (a.randomTerrainChance < 0 || a.randomTerrainChance > 100) {
			std::cerr << "Bad value for randomTerrainChance: expected an integer between 0 and 100, got: " << a.randomTerrainChance << "\n";
			exit(1);
		}

		if (a.leftVip && a.leftHar) {
			std::cerr << "Bad value for leftHar: cannot be combined with leftVip\n";
			exit(1);
		}

		if (a.leftUniformChance > 0 && (a.leftVip || a.leftHar)) {
			std::cerr << "Bad value for leftUniformChance: cannot be combined with left VIP or HAR\n";
			exit(1);
		}

		if (a.rightUniformChance > 0 && (a.rightVip || a.rightHar)) {
			std::cerr << "Bad value for rightUniformChance: cannot be combined with right VIP or HAR\n";
			exit(1);
		}

		if (a.rightVip && a.rightHar) {
			// This introduces a conflict in random HAR army generation (where HAR army must have faster units than the enemy)
			std::cerr << "Bad value for rightHar: cannot be combined with rightVip\n";
			exit(1);
		}

		if (a.leftHar && a.rightHar) {
			std::cerr << "Bad HAR configuration: both sides cannot be HAR opponents\n";
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

		// XXX: can this blow given preinitDLL is not yet called here?
		auto dir = VCMIDirs::get().userDataPath() / "Maps";
		validateFile("map", a.mapname, VCMIDirs::get().userDataPath() / "Maps");

		if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), a.loglevelAI) == LOGLEVELS.end()) {
			std::cerr << "Bad value for loglevelAI: " << a.loglevelAI << "\n";
			exit(1);
		}

		if (std::find(LOGLEVELS.begin(), LOGLEVELS.end(), a.loglevelNetwork) == LOGLEVELS.end()) {
			std::cerr << "Bad value for loglevelNetwork: " << a.loglevelNetwork << "\n";
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


		if (a.temperature < 0) {
			std::cerr << "Bad value for temperature: expected a non-negative float, got: " << a.temperature << "\n";
			exit(1);
		}
	}

	void processArguments(
		MMAI::Schema::IModel * leftModel,
		MMAI::Schema::IModel * rightModel,
		const InitArgs & a)
	{
		headless = a.headless;
		hotseat = a.hotseat && !headless;
		baggage = new MMAI::Schema::Baggage;

		baggage->modelLeft = leftModel;
		baggage->modelRight = rightModel;
		baggage->seed = a.seed;
		baggage->temperature = a.temperature;
		baggage->useCreatureSpells = a.useCreatureSpells;

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
		Settings(settings.write({"server", "ML", "randomArmies"}))->Bool() = a.randomArmies;
		Settings(settings.write({"server", "ML", "mirrorArmies"}))->Bool() = a.mirrorArmies;
		Settings(settings.write({"server", "ML", "randomArmyValueMin"}))->Integer() = a.randomArmyValueMin;
		Settings(settings.write({"server", "ML", "randomArmyValueMax"}))->Integer() = a.randomArmyValueMax;
		Settings(settings.write({"server", "ML", "randomArmyTargetVar"}))->Integer() = a.randomArmyTargetVar;
		Settings(settings.write({"server", "ML", "leftTargetMod"}))->Float() = a.leftTargetMod;
		Settings(settings.write({"server", "ML", "rightTargetMod"}))->Float() = a.rightTargetMod;
		Settings(settings.write({"server", "ML", "leftUniformChance"}))->Integer() = a.leftUniformChance;
		Settings(settings.write({"server", "ML", "rightUniformChance"}))->Integer() = a.rightUniformChance;
		Settings(settings.write({"server", "ML", "leftWhitelist"}))->String() = a.leftWhitelist;
		Settings(settings.write({"server", "ML", "rightWhitelist"}))->String() = a.rightWhitelist;
		Settings(settings.write({"server", "ML", "tightFormationChance"}))->Integer() = a.tightFormationChance;
		Settings(settings.write({"server", "ML", "creatureBankChance"}))->Integer() = a.creatureBankChance;
		Settings(settings.write({"server", "ML", "randomTerrainChance"}))->Integer() = a.randomTerrainChance;
		Settings(settings.write({"server", "ML", "leftVip"}))->Bool() = a.leftVip;
		Settings(settings.write({"server", "ML", "rightVip"}))->Bool() = a.rightVip;
		Settings(settings.write({"server", "ML", "leftHar"}))->Bool() = a.leftHar;
		Settings(settings.write({"server", "ML", "rightHar"}))->Bool() = a.rightHar;
		Settings(settings.write({"server", "ML", "battlefieldPattern"}))->String() = a.battlefieldPattern;
		Settings(settings.write({"server", "ML", "manaMin"}))->Integer() = a.manaMin;
		Settings(settings.write({"server", "ML", "manaMax"}))->Integer() = a.manaMax;
		Settings(settings.write({"server", "ML", "randomPrimarySkills"}))->Integer() = a.randomPrimarySkills;
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

		// Explicitly set neutral battle AI (AAI is not used to create it)
		if (rightModel->getName() == "BattleAI" || rightModel->getName() == "StupidAI")
			Settings(settings.write({"ai", "combatNeutralAI"}))->String() = rightModel->getName();
		else
			Settings(settings.write({"ai", "combatNeutralAI"}))->String() = "MMAI";

		// With GUI, the player's "adventure" AI is CPlayerInterface
		// When auto-combat is pressed, it creates whatever combatAlliedAI says
		Settings(settings.write({"ai", "combatAlliedAI"}))->String() = "MMAI";

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
		conflog("network", a.loglevelNetwork);
		conflog("mod", loglevelMod);
		conflog("animation", loglevelAnimation);
		conflog("bonus", loglevelBonus);
	}

	void init_vcmi(
		MMAI::Schema::IModel * leftModel,
		MMAI::Schema::IModel * rightModel,
		const InitArgs & a)
	{
		// Store original shell workdir (as VCMI will chdir to VCMI_BIN_DIR)
		// The original workdir is used for loading models specified by relative paths
		// (then is again changed to VCMI_BIN_DIR to prevent VCMI errors)
		auto wd = fs::current_path();

		// chdir needed for VCMI init
		fs::current_path(fs::path(VCMI_BIN_DIR));
		std::cout.flags(std::ios::unitbuf);
		console = new CConsoleHandler();
		console->start();

		const boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
		logConfig = new CBasicLogConfigurator(logPath, console);
		logConfig->configureDefault();

		initializeBaseFilesystem(false);
		auto library = std::make_unique<GameLibrary>();
		library->initializeFilesystem();

		validateArguments(a);
		processArguments(leftModel, rightModel, a);

		// printf("map: %s\n", map.c_str());
		// printf("loglevelGlobal: %s\n", loglevelGlobal.c_str());
		// printf("loglevelAI: %s\n", loglevelAI.c_str());
		// printf("headless: %d\n", headless);

		Settings(settings.write({"battle", "speedFactor"}))->Integer() = 5;
		Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
		Settings(settings.write({"battle", "stickyHeroInfoWindows"}))->Bool() = false;
		Settings(settings.write({"logging", "console", "format"}))->String() = "[%p/%t][%n] %l %m";
		Settings(settings.write({"logging", "console", "coloredOutputEnabled"}))->Bool() = true;
		Settings(settings.write({"logging", "console", "threshold"}))->String() = "trace";

		Settings(settings.write({"session", "disableVideo"}))->Bool() = true;
		Settings(settings.write({"video", "fullscreen"}))->Bool() = false;
		Settings(settings.write({"battle", "cellBorders"}))->Bool() = true;
		Settings(settings.write({"battle", "rangeLimitHighlightOnHover"}))->Bool() = true;
		Settings(settings.write({"battle", "speedFactor"}))->Integer() = 18;
		Settings(settings.write({"gameTweaks", "skipBattleIntroMusic"}))->Bool() = true;

		// logGlobal->debug("settings = %s", settings.toJsonNode().toJson());

		logConfig->configure();

		// XXX: EntryPoint does NOT init ENGINE if headless, but I don't think it can work without it
		// TODO: add flag in GameEngine to skip sound and music init if headless
		ENGINE = std::make_unique<GameEngine>(headless);

		auto ap = AutocombatPreferences();
		ap.other = std::make_any<MMAI::Schema::Baggage*>(baggage);
		GAME = std::make_unique<GameInstance>(ap);

		if (!headless)
			ENGINE->setEngineUser(GAME.get());

		std::thread loading([&library]() {
			try
			{
				CStopWatch tmh;
				library->initializeLibrary();
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

		setGameLibrary(std::move(library));

		// Model paths can refer to resources in the active library's mods.
		for (auto &model : {leftModel, rightModel}) {
			if (model->getType() != MMAI::Schema::ModelType::PATH)
				continue;

			auto rpath = ResourcePath(model->getName());
			auto loaders = CResourceHandler::get()->getResourcesWithName(rpath);

			if (loaders.size() != 1) {
				std::cerr << "Bad torch model path: " << model->getName() << "\nMust be a valid VCMI filesystem path, e.g. MMAI/models/defender-v5.pt";
				exit(1);
			}
		}

		if (!headless) {
			ENGINE->initAudio();
			graphics = new Graphics(); // should be before curh
			ENGINE->renderHandler().onLibraryLoadingFinished(LIBRARY);
			CMessage::init();
			ENGINE->cursor().init();
			ENGINE->cursor().show();
		}
	}

	namespace {
		void terminate_handler() {
			if (auto eptr = std::current_exception()) {
				try {
					std::rethrow_exception(eptr);
				} catch (const std::exception& e) {
					logGlobal->error("Exception: " + std::string(e.what()));
				} catch (...) {
					logGlobal->error("Exception: unknown");
				}
			}

			std::cerr << boost::stacktrace::stacktrace() << "\n";
			clearPendingTerminalInput();
			std::abort();
		}
	}

	std::shared_ptr<const BattleCheckpoint> capture_server_checkpoint()
	{
		if(!headless || !GAME)
			throw std::logic_error("Battle checkpoint requires a headless ML client");

		auto & handler = GAME->server();
		if(!handler.client || handler.client->gameState().currentBattles.size() != 1)
			throw std::logic_error("Battle checkpoint requires exactly one active client battle");
		return handler.captureBattleCheckpoint(handler.client->gameState().currentBattles.front()->getBattleID());
	}

	class DecisionCheckpoint
	{
	public:
		std::weak_ptr<CGameState> clientState;
		BattleID battleID;
		PlayerColor player;
		std::shared_ptr<const BattleCheckpoint> clientBattle;
		std::shared_ptr<const BattleCheckpoint> serverBattle;
		std::vector<std::function<void()>> controllers;
		std::vector<std::byte> clientRng;
	};

	std::shared_ptr<const DecisionCheckpoint> checkpoint(int player)
	{
		if(!headless || !GAME)
			throw std::logic_error("Battle checkpoint requires a headless ML client");
		auto & handler = GAME->server();
		if(!handler.client)
			throw std::logic_error("Battle checkpoint requires an active client");
		auto & client = *handler.client;
		auto & state = client.gameState();
		if(state.currentBattles.size() != 1 || client.currentBattleAction)
			throw std::logic_error("Battle checkpoint requires one active battle between actions");

		const auto & battle = *state.currentBattles.front();
		const auto * unit = battle.battleActiveUnit();
		const auto color = PlayerColor(player);
		if(!unit || battle.getTacticDist() || battle.battleGetOwner(unit) != color)
			throw std::logic_error("Battle checkpoint requires the current player's combat decision");

		auto saved = std::make_shared<DecisionCheckpoint>();
		saved->clientState = client.gameStatePtr();
		saved->battleID = battle.getBattleID();
		saved->player = color;
		for(BattleSide side : {BattleSide::ATTACKER, BattleSide::DEFENDER})
		{
			const auto it = client.battleints.find(battle.getSidePlayer(side));
			if(it == client.battleints.end())
				throw std::logic_error("Both battle controllers must be hosted by the local client");
			auto restore = it->second->checkpointBattleController();
			if(!restore)
				throw std::logic_error("Battle controller does not support checkpoints");
			saved->controllers.push_back(std::move(restore));
		}
		saved->clientBattle = captureClientBattle(state, saved->battleID);
		CMemorySerializer rng;
		rng.oser & CRandomGenerator::getDefault();
		saved->clientRng = rng.extractBuffer();

		auto & clientLoop = handler.getNetworkHandler().getContext();
		handler.runOnLocalGameServer([saved, &handler, &clientLoop](CGameHandler & server) {
			saved->serverBattle = server.mlplugin->captureCheckpoint(saved->battleID);
			server.mlplugin->beginRollout(saved->battleID, [&handler, &clientLoop, id = saved->battleID, color = saved->player, session = saved->clientState](RolloutResult outcome) {
				boost::asio::post(clientLoop, [&handler, id, color, session, outcome] {
					std::shared_ptr<CBattleGameInterface> controller;
					{
						std::unique_lock<std::mutex> interfaceLock;
						if(ENGINE)
							interfaceLock = std::unique_lock<std::mutex>(ENGINE->interfaceMutex);
						if(!handler.client || session.lock() != handler.client->gameStatePtr() || !handler.client->gameState().getBattle(id))
							return;
						controller = handler.client->battleints.at(color);
					}
					BattleResult result;
					result.battleID = id;
					result.result = outcome.result;
					result.winner = outcome.winner;
					controller->battleRolloutEnded(result);
				});
			});
		});
		return saved;
	}

	void restore_checkpoint(std::shared_ptr<const DecisionCheckpoint> saved, bool finish)
	{
		auto & handler = GAME->server();
		if(!saved || !handler.client || saved->clientState.lock() != handler.client->gameStatePtr()
			|| !handler.client->gameState().getBattle(saved->battleID))
			throw std::logic_error("Checkpoint does not belong to the current decision session");

		handler.runOnLocalGameServer([saved, finish](CGameHandler & server) {
			if(finish)
				server.mlplugin->finishRollout(*saved->serverBattle);
			else
				server.mlplugin->restore(*saved->serverBattle);
		});

		// The active callback still holds stack references. Replace stacks only after it returns.
		boost::asio::post(handler.getNetworkHandler().getContext(), [&handler, saved] {
			std::unique_lock<std::mutex> interfaceLock;
			if(ENGINE)
				interfaceLock = std::unique_lock<std::mutex>(ENGINE->interfaceMutex);
			if(!handler.client || saved->clientState.lock() != handler.client->gameStatePtr()
				|| !handler.client->gameState().getBattle(saved->battleID))
				return;
			auto & client = *handler.client;
			restoreClientBattle(client.gameState(), *saved->clientBattle);
			for(const auto & restore : saved->controllers)
				restore();
			CMemorySerializer rng(saved->clientRng);
			rng.iser & CRandomGenerator::getDefault();
			client.startPlayerBattleAction(saved->battleID, saved->player);
		});
	}

	void start_vcmi() {
		if (mapname == "")
			throw std::runtime_error("call init_vcmi first");

		std::set_terminate(terminate_handler);

		logGlobal->info("friendlyAI -> " + settings["ai"]["combatAlliedAI"].String());
		logGlobal->info("playerAI -> " + settings["ai"]["adventureEnemyAI"].String());
		logGlobal->info("enemyAI -> " + settings["ai"]["combatEnemyAI"].String());
		logGlobal->info("headless -> " + std::to_string(settings["session"]["headless"].Bool()));
		logGlobal->info("onlyai -> " + std::to_string(settings["session"]["onlyai"].Bool()));
		logGlobal->info("quickCombat -> " + std::to_string(settings["adventure"]["quickCombat"].Bool()));

		auto &si = GAME->server();
		if (headless) {
			auto l = std::unique_lock(mutex_shutdown);
			auto t = std::thread([&si]() {
				si.debugStartTest(mapname, false);
			});
			cond_shutdown.wait(l);
			std::cout << "VCMI shutdown complete.\n";
			t.join();
		} else {
			// GAME->mainmenu()->makeActiveInterface();
			if (hotseat) {
				const std::vector<std::string> playerNames = {"Player 1", "Player 2"};
				si.debugStartTest(mapname, false, playerNames, true);
			} else {
				si.debugStartTest(mapname, false);
			}

			ENGINE->mainLoop();
		}

		shutdown_vcmi();
		clearPendingTerminalInput();
		quitApplicationImmediately(0);
	}
}
