/*
 * graph.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/element_store.h"
#include "BAI/v15/graph/edges/generic.h"
#include "BAI/v15/graph/edges/hex_adjacent_hex.h"
#include "BAI/v15/graph/edges/unit_acts_before_unit.h"
#include "BAI/v15/graph/edges/unit_melee_dmg_unit.h"
#include "BAI/v15/graph/edges/unit_ranged_dmg_unit.h"
#include "BAI/v15/graph/nodes/action.h"
#include "BAI/v15/graph/nodes/global.h"
#include "BAI/v15/graph/nodes/hex.h"
#include "BAI/v15/graph/nodes/player.h"
#include "BAI/v15/graph/nodes/unit.h"

namespace MMAI::BAI::V15::Graph
{

struct GraphStore
{
    std::tuple<
        ElementStore<Nodes::Action>,
        ElementStore<Nodes::Global>,
        ElementStore<Nodes::Player>,
        ElementStore<Nodes::Unit>,
        ElementStore<Nodes::Hex>,
        ElementStore<Edges::Action_ExposesTo_Unit>,
        ElementStore<Edges::Action_Threatens_Unit>,
        ElementStore<Edges::Action_Damages_Unit>,
        ElementStore<Edges::Action_EndsAt_Hex>,
        ElementStore<Edges::Action_By_Unit>,
        ElementStore<Edges::Unit_Blocks_Unit>,
        ElementStore<Edges::Unit_MeleeDmg_Unit>,
        ElementStore<Edges::Unit_RangedDmg_Unit>,
        ElementStore<Edges::Unit_CanMelee_Unit>,
        ElementStore<Edges::Unit_CanShoot_Unit>,
        ElementStore<Edges::Unit_ActsBefore_Unit>,
        ElementStore<Edges::Unit_Threatens_Hex>,
        ElementStore<Edges::Unit_Occupies_Hex>,
        ElementStore<Edges::Hex_Adjacent_Hex>
    > stores;

    static_assert(EU(S15::Graph::ElementType::_count) == 19);
};

}
