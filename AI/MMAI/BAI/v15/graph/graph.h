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

#include "BAI/v15/graph/node_store.h"
#include "BAI/v15/graph/edge_store.h"
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
#include <tuple>
#include <unordered_map>

namespace MMAI::BAI::V15::Graph
{

namespace detail
{
    using TNodeStores = std::tuple<
        NodeStore<Nodes::Action>,
        NodeStore<Nodes::Global>,
        NodeStore<Nodes::Player>,
        NodeStore<Nodes::Unit>,
        NodeStore<Nodes::Hex>
    >;

    using TEdgeStores = std::tuple<
        EdgeStore<Edges::Action_ExposesTo_Unit>,
        EdgeStore<Edges::Action_Threatens_Unit>,
        EdgeStore<Edges::Action_Damages_Unit>,
        EdgeStore<Edges::Action_EndsAt_Hex>,
        EdgeStore<Edges::Action_By_Unit>,
        EdgeStore<Edges::Unit_Blocks_Unit>,
        EdgeStore<Edges::Unit_MeleeDmg_Unit>,
        EdgeStore<Edges::Unit_RangedDmg_Unit>,
        EdgeStore<Edges::Unit_CanMelee_Unit>,
        EdgeStore<Edges::Unit_CanShoot_Unit>,
        EdgeStore<Edges::Unit_ActsBefore_Unit>,
        EdgeStore<Edges::Unit_Threatens_Hex>,
        EdgeStore<Edges::Unit_Occupies_Hex>,
        EdgeStore<Edges::Hex_Adjacent_Hex>
    >;

    static_assert(
        EU(S15::Graph::ElementType::_count) ==
            std::tuple_size<TNodeStores>() + std::tuple_size<TEdgeStores>()
    );

    template <typename T, typename Tuple>
    struct tuple_contains;

    template <typename T, typename... Ts>
    struct tuple_contains<T, std::tuple<Ts...>>
        : std::bool_constant<(std::same_as<T, Ts> || ...)> {};

    template <typename T>
    constexpr bool is_stored_node_v =
        tuple_contains<NodeStore<std::remove_cvref_t<T>>, TNodeStores>::value;

    template <typename T>
    constexpr bool is_stored_edge_v =
        tuple_contains<EdgeStore<std::remove_cvref_t<T>>, TEdgeStores>::value;

    template <typename>
    inline constexpr bool always_false_v = false;
}

namespace S15 = Schema::V15;

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

    // "Perfect" forwarding function which preserves lvalue/rvalue category:
    // lvalues are copied, rvalues are moved.
    template <typename T>
    void add(T&& elem)
    {
        getMutableStore<T>().add(std::forward<T>(elem));
    }

    template <typename T>
    auto getById(std::size_t ind) const
    {
        return getStore<T>().getById(ind);
    }

    // For lookup, you usually do not want forwarding, because lookup should
    // not consume or mutate the argument. Prefer a const reference.
    template <typename T>
    auto getByIdentity(const T & elem) const
    {
        return getStore<T>().getByIdentity(elem);
    }

    template <typename T, typename Key>
        requires (!std::is_same_v<typename T::extra_index_type, void>)
    auto getByExtraIndex(const Key & key) const
    {
        return getStore<T>().getByExtraIndex(key);
    }

    template <typename T>
    auto getAll() const
    {
        return getStore<T>().entries();
    }

    template <typename EdgeT, typename NodeT>
    auto getAllEdgesBySrc(const NodeT & src) const
    {
        return getStore<EdgeT>().bySrc(src);
    }

    template <typename EdgeT, typename NodeT>
    auto getAllEdgesByDst(const NodeT & src) const
    {
        return getStore<EdgeT>().byDst(src);
    }

    template <typename T>
    auto size() const
    {
        return getStore<T>().size();
    }

    template <typename T>
    const auto& getStore() const
    {
        return getMutableStore<T>();
    }

    std::vector<const S15::Graph::INode*>
    getNodes(S15::Graph::ElementType t) const override;

    std::vector<const S15::Graph::IEdge*>
    getEdges(S15::Graph::ElementType t) const override;

    const AccessibilityInfo & getAccessibility() const;
    const Nodes::Unit::Queue & getQueue() const;

    const ReachabilityInfo & getReachability(const CStack & cstack) const;
    bool isRUFR(const CStack & cstack, const BattleHex & bh) const;

    // Explicitly building caches allows to define getters as const.
    void buildAccessibilityCache();
    void buildQueueCache(bool isMorale);
    void buildReachabilityCache();
    void buildNeighbouringStacksCache();
private:
    bool haveAccessibilityCache = false;
    bool haveQueueCache = false;
    bool haveReachabilityCache = false;

    const CPlayerBattleCallback & battle;
    const CStack * acstack;  // can be nullptr

    detail::TNodeStores nodeStores;
    detail::TEdgeStores edgeStores;

    std::unordered_map<uint32_t, ReachabilityInfo> rcache;
    std::unordered_map<uint32_t, std::array<bool, GameConstants::BFIELD_SIZE>> rufrHexes;
    std::unique_ptr<AccessibilityInfo> acache;
    std::unique_ptr<Nodes::Unit::Queue> queue;

    template <typename T>
    auto& getMutableStore() const
    {
        using U = std::remove_cvref_t<T>;
        if constexpr (detail::is_stored_node_v<U>)
            return std::get<NodeStore<U>>(nodeStores);
        else if constexpr (detail::is_stored_edge_v<U>)
            return std::get<EdgeStore<U>>(edgeStores);
        else
            static_assert(detail::always_false_v<U>, "type is not a stored node/edge");
    }
};
}
