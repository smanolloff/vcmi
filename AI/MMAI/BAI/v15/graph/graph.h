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
#include "BAI/v15/graph/edges/action_ends_at_hex.h"
#include "BAI/v15/graph/edges/hex_adjacent_hex.h"
#include "BAI/v15/graph/edges/unit_acts_before_unit.h"
#include "BAI/v15/graph/edges/unit_melee_dmg_unit.h"
#include "BAI/v15/graph/edges/unit_shoot_dmg_unit.h"
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
        NodeStore<Nodes::Global>,
        NodeStore<Nodes::Player>,
        NodeStore<Nodes::Unit>,
        NodeStore<Nodes::Hex>,
        NodeStore<Nodes::Action>,
        NodeStore<Nodes::Actaction>
    >;

    using TEdgeStores = std::tuple<
        EdgeStore<Edges::Hex_Adjacent_Hex>,
        EdgeStore<Edges::Unit_ActsBefore_Unit>,
        EdgeStore<Edges::Unit_MeleeDmg_Unit>,
        EdgeStore<Edges::Unit_ShootDmg_Unit>,
        EdgeStore<Edges::Unit_Blocks_Unit>,
        EdgeStore<Edges::Unit_Occupies_Hex>,
        EdgeStore<Edges::Action_By_Unit>,
        EdgeStore<Edges::Action_Blocks_Unit>,
        EdgeStore<Edges::Action_EndsAt_Hex>,
        EdgeStore<Edges::Action_ExposesToMeleeFrom_Unit>,
        EdgeStore<Edges::Action_ExposesToShootFrom_Unit>,
        EdgeStore<Edges::Action_Melees_Unit>,
        EdgeStore<Edges::Action_Shoots_Unit>,
        EdgeStore<Edges::Action_Threatens_Unit>,
        EdgeStore<Edges::Actaction_By_Unit>,
        EdgeStore<Edges::Actaction_Blocks_Unit>,
        EdgeStore<Edges::Actaction_EndsAt_Hex>,
        EdgeStore<Edges::Actaction_ExposesToMeleeFrom_Unit>,
        EdgeStore<Edges::Actaction_ExposesToShootFrom_Unit>,
        EdgeStore<Edges::Actaction_Melees_Unit>,
        EdgeStore<Edges::Actaction_Shoots_Unit>,
        EdgeStore<Edges::Actaction_Threatens_Unit>,
        EdgeStore<Edges::Actaction_Threatens_Hex>
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

    // XXX: pass-by-value + move is preferred to pass-by-reference
    //      => must accept non-const std::shared ptr
    // XXX: The stores hold pointers-to-const, but template deduction fails
    //      => must accept pointer-to-non-const here, but specify it upstream
    //          (to avoid having to specify it at the G->add call site)
    template <typename T>
    void add(std::shared_ptr<T> elem)
    {
        getMutableStore<T>().add(std::shared_ptr<const T>{std::move(elem)});
    }

    template <typename T>
    std::shared_ptr<const T> getById(std::size_t ind) const
    {
        return getStore<T>().getById(ind);
    }

    template <typename T>
    std::shared_ptr<const T> getByIdentity(const std::shared_ptr<const T> & elem) const
    {
        return getStore<T>().getByIdentity(elem);
    }

    template <typename T, typename Key>
        requires (!std::is_same_v<typename T::extra_index_type, void>)
    std::shared_ptr<const T> getByExtraIndex(const Key & key) const
    {
        return getStore<T>().getByExtraIndex(key);
    }

    template <typename T>
    const auto & getAll() const
    {
        return getStore<T>().entries();
    }

    template <typename EdgeType, typename NodeType>
    auto getAllEdgesBySrc(const std::shared_ptr<const NodeType> & src) const
    {
        return getStore<EdgeType>().getAllBySrc(src);
    }

    template <typename EdgeType, typename NodeType>
    auto getAllEdgesByDst(const std::shared_ptr<const NodeType> & dst) const
    {
        return getStore<EdgeType>().getAllByDst(dst);
    }

    template <typename T>
    auto size() const
    {
        return getStore<T>().size();
    }

    template <typename T>
    std::ptrdiff_t getId(const T & elem) const
    {
        return getStore<T>().getId(elem);
    }

    template <typename T>
    const auto & getStore() const
    {
        using U = std::remove_cvref_t<T>;
        if constexpr (detail::is_stored_node_v<U>)
            return std::get<NodeStore<U>>(nodeStores);
        else if constexpr (detail::is_stored_edge_v<U>)
            return std::get<EdgeStore<U>>(edgeStores);
        else
            static_assert(detail::always_false_v<U>, "type is not a stored node/edge");
    }

    std::vector<const S15::Graph::INode*>
    getNodes(S15::Graph::ElementType t) const override;

    std::vector<const S15::Graph::IEdge*>
    getEdges(S15::Graph::ElementType t) const override;

    const AccessibilityInfo & getAccessibility() const;

    const ReachabilityInfo & getReachability(const CStack & cstack) const;
    bool isRUFR(const CStack & cstack, const BattleHex & bh) const;

    // Explicitly building caches allows to define getters as const.
    void buildAccessibilityCache();
    void buildReachabilityCache();
private:
    bool haveAccessibilityCache = false;
    bool haveReachabilityCache = false;

    const CPlayerBattleCallback & battle;
    const CStack * acstack;  // can be nullptr

    detail::TNodeStores nodeStores;
    detail::TEdgeStores edgeStores;

    std::unique_ptr<AccessibilityInfo> acache;
    std::unordered_map<uint32_t, ReachabilityInfo> rcache;
    std::unordered_map<uint32_t, std::array<bool, GameConstants::BFIELD_SIZE>> rufrHexes;

    // identical to getStore(), but returned type is non-const
    template <typename T>
    auto & getMutableStore()
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
