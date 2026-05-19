#pragma once

#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

namespace detail
{
	using Unit_ShootDmg_Unit_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::Unit_ShootDmg_Unit>;
	using Unit_ShootDmg_Unit_Base = Base<Nodes::Unit, Nodes::Unit, Unit_ShootDmg_Unit_Traits>;
}

class Unit_ShootDmg_Unit : public detail::Unit_ShootDmg_Unit_Base
{
public:
	Unit_ShootDmg_Unit(
		const std::shared_ptr<const Nodes::Unit> & srcNode,
		const std::shared_ptr<const Nodes::Unit> & dstNode,
	    int vdiffAttacker,
	    int vdiffDefender,
	    int hpdiffAttacker,
	    int hpdiffDefender,
		int battlefieldValue,
		int battlefieldHp
	);
};
}
