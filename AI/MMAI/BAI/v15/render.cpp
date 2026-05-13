/*
 * render.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "BAI/v15/render.h"

namespace MMAI::BAI::V15
{

// This function used during model development and is never called otherwise
void Verify(const State * state) // NOSONAR - function used for debugging only
{
    throw std::runtime_error("not implemented");
}

// This intentionally uses the IState interface to ensure that
// the schema is properly exposing all needed informaton
std::string Render(const Schema::IState * istate, const Action * action) // NOSONAR - function used for debugging only
{
    throw std::runtime_error("not implemented");
}

}
