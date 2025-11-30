/*
 * router.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "VCMIDirs.h"
#include "callback/CBattleCallback.h"
#include "callback/CDynLibHandler.h"
#include "callback/IGameInfoCallback.h"
#include "filesystem/Filesystem.h"
#include "json/JsonUtils.h"

#include "AI/BattleAI/BattleAI.h"
#include "AI/StupidAI/StupidAI.h"
#include "BAI/base.h"
#include "BAI/model/NNModel.h"
#include "BAI/model/ScriptedModel.h"
#include "BAI/router.h"

#include "common.h"
#include "gameState/CGameState.h"
#include "schema/schema.h"

#include <utility>

namespace MMAI::BAI
{
using ConfigStorage = std::map<std::string, std::string>;
using ModelStorage = std::map<std::string, std::unique_ptr<NNModel>>;

struct Config
{
	ConfigStorage modelconfig;
	ModelStorage models;
	float temperature = 1.0f;
	uint64_t seed = 0;
	std::unique_ptr<ScriptedModel> fallbackModel;
	std::mutex mutex;
};

static void InitModelConfigFromSettings()
{
	static Config cfg = Config();

	auto lock = std::lock_guard(cfg.mutex);
	if(!cfg.modelconfig.empty())
		return;

	auto warncfg = [](std::string problem)
	{
		logAi->warn("MMAI config error: %s", std::move(problem));
	};

	auto jsonConfig = JsonUtils::assembleFromFiles("MMAI/CONFIG/mmai-settings.json");

	if(!jsonConfig.isStruct())
	{
		logAi->error("Could not load MMAI config. Is MMAI mod enabled?");
		return;
	}

	auto loaded = jsonConfig.Struct();

	if(loaded["temperature"].isNumber())
	{
		if(loaded["temperature"].Float() < 0)
		{
			warncfg("temperature: value is negative");
		}
		else
		{
			cfg.temperature = static_cast<float>(loaded["temperature"].Float());
		}
	}
	else
	{
		warncfg("temperature: not a number");
	}

	if(loaded["seed"].getType() == JsonNode::JsonType::DATA_INTEGER)
	{
		if(loaded["seed"].Integer() < 0)
		{
			warncfg("seed: value is negative");
		}
		else
		{
			cfg.seed = static_cast<uint64_t>(loaded["seed"].Integer());
		}
	}
	else
	{
		warncfg("seed: not an integer");
	}

	if(loaded["models"].getType() != JsonNode::JsonType::DATA_STRUCT)
	{
		warncfg("seed: not a struct");
	}
	else
	{
		for(const auto & key : {"attacker", "defender"})
		{
			if(loaded["models"][key].isString())
			{
				std::string value = loaded["models"][key].String();
				value = "MMAI/models/" + value;
				if(!boost::algorithm::ends_with(value, ".onnx"))
				{
					value += ".onnx";
				}

				cfg.modelconfig.insert({key, value});
			}
			else
			{
				warncfg(std::string(key) + ": not a string");
			}
		}
	}

	if(loaded["fallback"].getType() != JsonNode::JsonType::DATA_STRING)
	{
		warncfg("fallback: not a string");
	}
	else
	{
		auto fallback = loaded["fallback"].String();
		if(fallback != "StupidAI" && fallback != "BattleAI")
		{
			warncfg("fallback: expected StupidAI or BattleAI, got: " + fallback);
		}
		else
		{
			cfg.modelconfig.insert({"fallback", fallback});
		}
	}
}

static Schema::IModel * GetModel(std::string key)
{
	auto & cfg = getConfig();

	try
	{
		auto lock = std::lock_guard(cfg.mutex);
		auto it = cfg.models.find(key);

		if(it == cfg.models.end())
		{
			auto it2 = cfg.modelconfig.find(key);
			if(it2 == cfg.modelconfig.end())
				THROW_FORMAT("No such key in model config: %s", key);

			logAi->debug("Found value for key %s: %s", key, it2->second);

			auto rpath = ResourcePath(it2->second);
			auto loaders = CResourceHandler::get()->getResourcesWithName(rpath);

			if(loaders.size() != 1)
			{
				THROW_FORMAT("Expected 1 %s loader, found %d", rpath.getName() % EI(loaders.size()));
			}

			auto fullpath = loaders.at(0)->getResourceName(rpath);
			ASSERT(fullpath.has_value(), "could not obtain path for resource " + rpath.getName());
			auto fullpathstr = fullpath.value().string();

			logAi->info("Loading MMAI %s model from %s", key, fullpathstr);
			it = cfg.models.try_emplace(key, std::make_unique<NNModel>(fullpathstr, cfg.temperature, cfg.seed)).first;
		}
		else
		{
			logAi->debug("Using previously loaded %s", key);
		}

		return it->second.get();
	}
	catch(std::exception & e)
	{
		logAi->error("Failed to load MMAI %s model: %s", key, e.what());

#ifdef ENABLE_MMAI_STRICT_LOAD
		throw;
#endif

		// XXX: unfortunately, there is no way to alert the user about
		// failures from within a combat ai
		auto it2 = cfg.modelconfig.find("fallback");
		std::string fb;

		if(it2 == cfg.modelconfig.end() || it2->second.empty())
		{
			logAi->warn("Fallback model not configured, defaulting to BattleAI");
			fb = "BattleAI";
		}
		else
		{
			fb = it2->second;
		}

		auto lock = std::lock_guard(cfg.mutex);
		if(!cfg.fallbackModel)
			cfg.fallbackModel = std::make_unique<ScriptedModel>(fb);

		logAi->info("Will use fallback model: %s", cfg.fallbackModel->getName());
		return cfg.fallbackModel.get();
	}
}

Router::Router()
{
	std::ostringstream oss;
	// Store the memory address and include it in logging
	// Convert the pointer value to an integer type
	std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(this);
	oss << std::hex << addr;
	addrstr = oss.str();
	info("+++ constructor +++"); // log after addrstr is set
}

Router::~Router()
{
	info("--- destructor ---");
	cb->waitTillRealize = wasWaitingForRealize;
}

void Router::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB)
{
	info("*** initBattleInterface ***");
	env = ENV;
	cb = CB;
	colorname = cb->getPlayerID()->toString();
	wasWaitingForRealize = cb->waitTillRealize;

	cb->waitTillRealize = false;
	bai.reset();
}

void Router::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences prefs)
{
	autocombatPreferences = prefs;
	initBattleInterface(ENV, CB);
}

/*
     * Delegated methods
     */

void Router::actionFinished(const BattleID & bid, const BattleAction & action)
{
	bai->actionFinished(bid, action);
}

void Router::actionStarted(const BattleID & bid, const BattleAction & action)
{
	bai->actionStarted(bid, action);
}

void Router::activeStack(const BattleID & bid, const CStack * astack)
{
	bai->activeStack(bid, astack);
}

void Router::battleAttack(const BattleID & bid, const BattleAttack * ba)
{
	bai->battleAttack(bid, ba);
}

void Router::battleCatapultAttacked(const BattleID & bid, const CatapultAttack & ca)
{
	bai->battleCatapultAttacked(bid, ca);
}

void Router::battleEnd(const BattleID & bid, const BattleResult * br, QueryID queryID)
{
	bai->battleEnd(bid, br, queryID);
}

void Router::battleGateStateChanged(const BattleID & bid, const EGateState state)
{
	bai->battleGateStateChanged(bid, state);
};

void Router::battleLogMessage(const BattleID & bid, const std::vector<MetaString> & lines)
{
	bai->battleLogMessage(bid, lines);
};

void Router::battleNewRound(const BattleID & bid)
{
	bai->battleNewRound(bid);
}

void Router::battleNewRoundFirst(const BattleID & bid)
{
	bai->battleNewRoundFirst(bid);
}

void Router::battleObstaclesChanged(const BattleID & bid, const std::vector<ObstacleChanges> & obstacles)
{
	bai->battleObstaclesChanged(bid, obstacles);
};

void Router::battleSpellCast(const BattleID & bid, const BattleSpellCast * sc)
{
	bai->battleSpellCast(bid, sc);
}

void Router::battleStackMoved(const BattleID & bid, const CStack * stack, const BattleHexArray & dest, int distance, bool teleport)
{
	bai->battleStackMoved(bid, stack, dest, distance, teleport);
}

void Router::battleStacksAttacked(const BattleID & bid, const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
	bai->battleStacksAttacked(bid, bsa, ranged);
}

void Router::battleStacksEffectsSet(const BattleID & bid, const SetStackEffect & sse)
{
	bai->battleStacksEffectsSet(bid, sse);
}

void Router::battleStart(
	const BattleID & bid,
	const CCreatureSet * army1,
	const CCreatureSet * army2,
	int3 tile,
	const CGHeroInstance * hero1,
	const CGHeroInstance * hero2,
	BattleSide side,
	bool replayAllowed
)
{
	Schema::IModel * model;
	InitModelConfigFromSettings();
	auto modelkey = side == BattleSide::ATTACKER ? "attacker" : "defender";
	model = GetModel(modelkey);

	auto modelside = model->getSide();
	auto realside = Schema::Side(EI(side));

	if(modelside != realside && modelside != Schema::Side::BOTH)
	{
		logAi->warn("The loaded '%s' model was not trained to play as %s", modelkey, modelkey);
	}

	// printf("(side=%d) hero0: %s, hero1: %s\n", EI(side), hero1->nameCustomTextId.c_str(), hero2->nameCustomTextId.c_str());

	switch(model->getType())
	{
		case Schema::ModelType::SCRIPTED:
			if(model->getName() == "StupidAI")
			{
				bai = CDynLibHandler::getNewBattleAI("StupidAI");
				bai->initBattleInterface(env, cb, autocombatPreferences);
			}
			else if(model->getName() == "BattleAI")
			{
				bai = CDynLibHandler::getNewBattleAI("BattleAI");
				bai->initBattleInterface(env, cb, autocombatPreferences);
			}
			else
			{
				THROW_FORMAT("Unexpected scripted model name: %s", model->getName());
			}
			break;
		case Schema::ModelType::NN:
			// XXX: must not call initBattleInterface here
			bai = Base::Create(model, env, cb, autocombatPreferences.enableSpellsUsage);
			break;

		default:
			THROW_FORMAT("Unexpected model type: %d", EI(model->getType()));
	}

	bai->battleStart(bid, army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void Router::battleTriggerEffect(const BattleID & bid, const BattleTriggerEffect & bte)
{
	bai->battleTriggerEffect(bid, bte);
}

void Router::battleUnitsChanged(const BattleID & bid, const std::vector<UnitChanges> & units)
{
	bai->battleUnitsChanged(bid, units);
}

void Router::yourTacticPhase(const BattleID & bid, int distance)
{
	bai->yourTacticPhase(bid, distance);
}

/*
     * private
     */

void Router::error(const std::string & text) const
{
	log(ELogLevel::ERROR, text);
}
void Router::warn(const std::string & text) const
{
	log(ELogLevel::WARN, text);
}
void Router::info(const std::string & text) const
{
	log(ELogLevel::INFO, text);
}
void Router::debug(const std::string & text) const
{
	log(ELogLevel::DEBUG, text);
}
void Router::trace(const std::string & text) const
{
	log(ELogLevel::TRACE, text);
}
void Router::log(ELogLevel::ELogLevel level, const std::string & text) const
{
	if(logAi->getEffectiveLevel() <= level)
		logAi->debug("Router-%s [%s] %s", addrstr, colorname, text);
}
}
