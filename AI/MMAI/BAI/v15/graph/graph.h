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

#pragma once

#include "battle/CPlayerBattleCallback.h"
#include "battle/ReachabilityInfo.h"
#include "battle/AccessibilityInfo.h"

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

#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph
{

namespace S15 = Schema::V15;

using TStores = std::tuple<
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
>;

namespace detail
{
    template <typename T, typename Tuple>
    struct tuple_contains;

    template <typename T, typename... Ts>
    struct tuple_contains<T, std::tuple<Ts...>>
        : std::bool_constant<(std::same_as<T, Ts> || ...)>
    {};

    template <typename T>
    constexpr bool is_stored_element_v =
        tuple_contains<
            ElementStore<std::remove_cvref_t<T>>,
            TStores
        >::value;
}


class Graph : public S15::Graph::IGraph
{

public:
    Graph(
        const CPlayerBattleCallback & battle,
        const CStack * acstack
    ) : battle(battle), acstack(acstack) {};

    Graph(const Graph &) = delete;
    Graph & operator=(const Graph &) = delete;
    Graph(Graph &&) = delete;
    Graph & operator=(Graph &&) = delete;

    template <typename T>
        requires detail::is_stored_element_v<T>
    const ElementStore<std::remove_cvref_t<T>> & getElementStore() const
    {
        using U = std::remove_cvref_t<T>;
        return std::get<ElementStore<U>>(stores);
    }

    // "Perfect" forwarding function which preserves lvalue/rvalue category:
    // lvalues are copied, rvalues are moved.
    template <typename T>
        requires detail::is_stored_element_v<T>
    void add(T&& elem)
    {
        using U = std::remove_cvref_t<T>;
        std::get<ElementStore<U>>(stores).add(std::forward<T>(elem));
    }

    template <typename T>
    void add(std::shared_ptr<T> elem)
    {
        using U = std::remove_cvref_t<T>;
        std::get<ElementStore<U>>(stores).add(std::forward<T>(elem));
    }

    template <typename T>
        requires detail::is_stored_element_v<T>
    const std::shared_ptr<std::remove_cvref_t<T>> & get(std::size_t ind) const
    {
        using U = std::remove_cvref_t<T>;
        return std::get<ElementStore<U>>(stores).get(ind);
    }

    template <typename T>
        requires detail::is_stored_element_v<T>
    auto getAll() const
    {
        using U = std::remove_cvref_t<T>;
        return std::get<ElementStore<U>>(stores).entries();
    }

    template <typename T>
        requires detail::is_stored_element_v<T>
    auto size() const
    {
        using U = std::remove_cvref_t<T>;
        return std::get<ElementStore<U>>(stores).size();
    }

    std::vector<const S15::Graph::INode*>
    getNodes(S15::Graph::ElementType t) const override;

    std::vector<const S15::Graph::IEdge*>
    getEdges(S15::Graph::ElementType t) const override;

    const AccessibilityInfo & getAccessibility() const;
    const Nodes::Unit::Queue & getQueue() const;
    const Nodes::Unit * findUnitByBHex(const BattleHex & bh) const;

    const ReachabilityInfo & getReachability(const CStack & cstack) const;
    bool isRUFR(const CStack & cstack, const BattleHex & bh) const;

    // Explicitly building caches allows to define getters as const.
    void buildAccessibilityCache();
    void buildQueueCache(bool isMorale);
    void buildUnitsByBHexCache();
    void buildReachabilityCache();
    void buildNeighbouringStacksCache();
private:
    bool haveAccessibilityCache = false;
    bool haveQueueCache = false;
    bool haveUnitsByBHexCache = false;
    bool haveReachabilityCache = false;

    const CPlayerBattleCallback & battle;
    const CStack * acstack;  // can be nullptr

    TStores stores;
    static_assert(EU(S15::Graph::ElementType::_count) == 19);

    std::unordered_map<uint32_t, ReachabilityInfo> rcache;
    std::unordered_map<uint32_t, std::array<bool, GameConstants::BFIELD_SIZE>> rufrHexes;
    std::unique_ptr<AccessibilityInfo> acache;
    std::unique_ptr<Nodes::Unit::Queue> queue;
    std::unordered_map<const BattleHex, const std::shared_ptr<Nodes::Unit>> unitsByBHex;
};
}
