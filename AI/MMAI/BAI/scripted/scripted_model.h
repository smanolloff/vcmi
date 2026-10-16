/*
 * scripted_model.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/base.h"

namespace MMAI::BAI
{

// SCRIPTED models are dummy models which should not be used for anything
// other than their getType() and getName() methods. Based on the return
// value, the corresponding scripted bot (e.g. StupidAI) should be
// used for the upcoming battle instead.
// When MMAI fails to load an ML model, it loads a SCRIPTED model instead
// as per MMAI mod's "fallback" setting in order to prevent a game crash.
class ScriptedModel : public MMAI::Schema::IModel
{
public:
	explicit ScriptedModel(const std::string & keyword);

	Schema::ModelType getType() override;
	std::string getName() override;
	int getVersion() override;
	int getAction(const MMAI::Schema::IState * s) override;
	Schema::Side getSide() override;
	double getValue(const MMAI::Schema::IState * s) override;

private:
	const std::string keyword;
	void warn(const std::string & m, int retval) const;
};
}
