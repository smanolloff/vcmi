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
#include "StdInc.h" // IWYU pragma: keep

#include "BAI/v15/graph/edges/action_melees_unit.h"
#include "BAI/v15/graph/edges/action_shoots_unit.h"
#include "battle/CPlayerBattleCallback.h"
#include "battle/ReachabilityInfo.h"
#include "battle/AccessibilityInfo.h"

#include "BAI/v15/enum_flags.h"
#include "BAI/v15/graph/node_store.h"
#include "BAI/v15/graph/edge_store.h"
#include "BAI/v15/graph/edges/generic.h"
#include "BAI/v15/graph/edges/action_ends_at_hex.h"
#include "BAI/v15/graph/edges/action_exposes_to_shoot_from_unit.h"
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
        NodeStore<Nodes::Action>
    >;

    using TEdgeStores = std::tuple<
        EdgeStore<Edges::Global_Has_Player>,
        EdgeStore<Edges::Global_Has_Unit>,
        EdgeStore<Edges::Global_Has_Hex>,
        EdgeStore<Edges::Player_Owns_Unit>,
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
        EdgeStore<Edges::Action_EnablesMeleeAt_Unit>,
        EdgeStore<Edges::Action_EnablesShootAt_Unit>,
        EdgeStore<Edges::Action_EnablesMeleeAt_Hex>,
        EdgeStore<Edges::Action_EnablesShootAt_Hex>
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
    concept is_stored_node =
        tuple_contains<NodeStore<std::remove_cvref_t<T>>, TNodeStores>::value;

    template <typename T>
    concept is_stored_edge =
        tuple_contains<EdgeStore<std::remove_cvref_t<T>>, TEdgeStores>::value;

    template <typename T>
    concept is_stored_element = is_stored_node<T> || is_stored_edge<T>;

    template <typename EdgeType, typename NodeType>
    concept is_edge_src = std::is_same_v<typename EdgeType::src_node_type, NodeType>;

    template <typename EdgeType, typename NodeType>
    concept is_edge_dst = std::is_same_v<typename EdgeType::dst_node_type, NodeType>;

    template <typename>
    inline constexpr bool always_false = false;
}

namespace S15 = Schema::V15;

class Graph : public S15::Graph::IGraph
{

public:
    explicit Graph(const CPlayerBattleCallback & battle);

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
    requires detail::is_stored_element<T>
    void add(std::shared_ptr<T> elem)
    {
        // constexpr if (std::is_)
        // std::cout << "DEBUG: Add: " << elem->name() << "\n";
        // if (!elem)
        //     throw std::runtime_error("add: nullptr given");
        assert(elem);
        getMutableStore<T>().add(std::move(elem));
    }

    template <typename T>
    std::shared_ptr<const T> getById(std::size_t ind, bool strict = true) const
    {
        return getStore<T>().getById(ind, strict);
    }

    template <typename T>
    requires detail::is_stored_element<T>
    std::shared_ptr<const T> getByIdentity(const std::shared_ptr<const T> & elem, bool strict = true) const
    {
        // if (!elem)
        //     throw std::runtime_error("getByIdentity: nullptr given");
        assert(elem);
        return getStore<T>().getByIdentity(elem, strict);
    }

    template <typename T, typename Key>
        requires (
            !std::is_same_v<typename T::extra_index_type, void> &&
            std::is_same_v<typename T::extra_index_type::result_type, Key>
        )
    std::shared_ptr<const T> getByExtraIndex(const Key & key, bool strict = true) const
    {
        return getStore<T>().getByExtraIndex(key, strict);
    }

    template <typename EdgeType, typename SrcNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_src<EdgeType, SrcNodeType>
    std::shared_ptr<const EdgeType> getOneEdgeBySrc(const std::shared_ptr<const SrcNodeType> & src, bool strict = true) const
    {
        // if (!src)
        //     throw std::runtime_error("getOneEdgeBySrc: nullptr given");
        assert(src);
        return getStore<EdgeType>().getOneBySrc(src, strict);
    }

    template <typename EdgeType, typename SrcNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_src<EdgeType, SrcNodeType>
    auto getOneEdgeDstBySrc(const std::shared_ptr<const SrcNodeType> & src, bool strict = true) const
    {
        // if (!src)
        //     throw std::runtime_error("getOneEdgeBySrc: nullptr given");
        assert(src);
        return getStore<EdgeType>().getOneDstBySrc(src, strict);
    }

    template <typename EdgeType, typename SrcNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_src<EdgeType, SrcNodeType>
    auto getAllEdgesBySrc(const std::shared_ptr<const SrcNodeType> & src) const
    {
        // if (!src)
        //     throw std::runtime_error("getAllEdgesBySrc: nullptr given");
        assert(src);
        return getStore<EdgeType>().getAllBySrc(src);
    }

    template <typename EdgeType, typename SrcNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_src<EdgeType, SrcNodeType>
    auto getAllEdgesDstBySrc(const std::shared_ptr<const SrcNodeType> & src) const
    {
        // if (!src)
        //     throw std::runtime_error("getAllEdgesBySrc: nullptr given");
        assert(src);
        return getStore<EdgeType>().getAllDstBySrc(src);
    }

    template <typename EdgeType, typename DstNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_dst<EdgeType, DstNodeType>
    std::shared_ptr<const EdgeType> getOneEdgeByDst(const std::shared_ptr<const DstNodeType> & dst, bool strict = true) const
    {
        // if (!dst)
        //     throw std::runtime_error("getOneEdgeByDst: nullptr given");
        assert(dst);
        return getStore<EdgeType>().getOneByDst(dst, strict);
    }

    template <typename EdgeType, typename DstNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_dst<EdgeType, DstNodeType>
    auto getOneEdgeSrcByDst(const std::shared_ptr<const DstNodeType> & dst, bool strict = true) const
    {
        // if (!dst)
        //     throw std::runtime_error("getOneEdgeByDst: nullptr given");
        assert(dst);
        return getStore<EdgeType>().getOneSrcByDst(dst, strict);
    }


    template <typename EdgeType, typename DstNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_dst<EdgeType, DstNodeType>
    auto getAllEdgesByDst(const std::shared_ptr<const DstNodeType> & dst) const
    {
        // if (!dst)
        //     throw std::runtime_error("getAllEdgesByDst: nullptr given");
        assert(dst);
        return getStore<EdgeType>().getAllByDst(dst);
    }

    template <typename EdgeType, typename DstNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_dst<EdgeType, DstNodeType>
    auto getAllEdgesSrcByDst(const std::shared_ptr<const DstNodeType> & dst) const
    {
        // if (!dst)
        //     throw std::runtime_error("getAllEdgesByDst: nullptr given");
        assert(dst);
        return getStore<EdgeType>().getAllSrcByDst(dst);
    }


    template <typename EdgeType, typename SrcNodeType, typename DstNodeType>
    requires detail::is_stored_edge<EdgeType>
        && detail::is_edge_src<EdgeType, SrcNodeType>
        && detail::is_edge_dst<EdgeType, DstNodeType>
    auto getEdgeBySrcDst(
        const std::shared_ptr<const SrcNodeType> & src,
        const std::shared_ptr<const DstNodeType> & dst,
        bool strict = true) const
    {
        // if (!src || !dst)
        //     throw std::runtime_error("getEdgeBySrcDst: nullptr given");
        assert(src && dst);
        return getStore<EdgeType>().getBySrcDst(src, dst, strict);
    }

    template <typename T>
    requires detail::is_stored_element<T>
    const auto & getAll() const
    {
        return getStore<T>().entries();
    }

    template <typename T>
    requires detail::is_stored_element<T>
    auto size() const
    {
        return getStore<T>().size();
    }

    template <typename T>
    requires detail::is_stored_element<T>
    std::ptrdiff_t getId(const std::shared_ptr<const T> & elem) const
    {
        // if (!elem)
        //     throw std::runtime_error("getAllEdgesByDst: nullptr given");
        assert(elem);
        return getStore<T>().getId(elem);
    }

    template <typename T>
    requires detail::is_stored_node<T>
    const auto & getStore() const
    {
        using U = std::remove_cvref_t<T>;
        return std::get<NodeStore<U>>(nodeStores);
    }

    template <typename T>
    requires detail::is_stored_edge<T>
    const auto & getStore() const
    {
        using U = std::remove_cvref_t<T>;
        return std::get<EdgeStore<U>>(edgeStores);
    }

    void verify() const;

    std::vector<const S15::Graph::INode*>
    getNodes(S15::Graph::ElementType t) const override;

    std::vector<const S15::Graph::IEdge*>
    getEdges(S15::Graph::ElementType t) const override;

    std::vector<std::tuple<int, int>>
    getActiveNodeToActionIds() const override;

    EnumFlags<S15::Graph::ElementType> getFlags() const;
    void setFlag(S15::Graph::ElementType et);

    const AccessibilityInfo & getAccessibility() const;
    const ReachabilityInfo & getReachability(const CStack & cstack) const;

    // Explicitly building caches allows to define getters as const.
    void buildAccessibilityCache();
    void buildReachabilityCache();
private:
    EnumFlags<S15::Graph::ElementType> flags;
    bool haveAccessibilityCache = false;
    bool haveReachabilityCache = false;

    const CPlayerBattleCallback & battle;

    detail::TNodeStores nodeStores;
    detail::TEdgeStores edgeStores;

    std::unique_ptr<AccessibilityInfo> acache;
    std::unordered_map<uint32_t, ReachabilityInfo> rcache;

    // identical to getStore(), but returned type is non-const
    template <typename T>
    requires detail::is_stored_node<T>
    auto & getMutableStore()
    {
        using U = std::remove_cvref_t<T>;
        return std::get<NodeStore<U>>(nodeStores);
    }

    template <typename T>
    requires detail::is_stored_edge<T>
    auto & getMutableStore()
    {
        using U = std::remove_cvref_t<T>;
        return std::get<EdgeStore<U>>(edgeStores);
    }
};
}
