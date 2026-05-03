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

#include "BAI/v15/graph/graph_store.h"
#include "battle/AccessibilityInfo.h"
#include "battle/CPlayerBattleCallback.h"
#include "battle/ReachabilityInfo.h"

namespace MMAI::BAI::V15::Graph
{

class Graph : public S15::Graph::IGraph
{
public:
    explicit Graph(
        const CPlayerBattleCallback & battle,
        const CStack * acstack
    ) : battle(battle), acstack(acstack) {};

    Graph(const Graph &) = delete;
    Graph & operator=(const Graph &) = delete;
    Graph(Graph &&) = delete;
    Graph & operator=(Graph &&) = delete;

    template <typename T>
    const ElementStore<T> & getElementStore() const
    {
        std::get<ElementStore<T>>(store.stores);
    }

    template <typename T>
    void add(T elem)
    {
        std::get<ElementStore<T>>(store.stores).add(std::move(elem));
    }

    template <typename T>
    auto getAll() const
    {
        return std::get<ElementStore<T>>(store.stores).entries();
    }

    template <typename T>
    const T& get(std::size_t ind) const
    {
        return std::get<ElementStore<T>>(store.stores).get(ind);
    }

    std::vector<const S15::Graph::INode*>
    getNodes(S15::Graph::ElementType t) const override;

    std::vector<const S15::Graph::IEdge*>
    getEdges(S15::Graph::ElementType t) const override;

    // Convenience wrappers for std::make_shared<...>
    // (for more meaningful compuler hints/errors)
    void addGlobalNode(
        S15::CombatResult result,
        int round,
        int valueStart,
        int hpStart,
        int value,
        int hp
    );

    void addPlayerNode(
        BattleSide side,
        int globalValueStart,
        int globalHpStart,
        int globalValuePrevRound,
        int globalHpPrevRound,
        int value,
        int hp,
        int dmgDealt,
        int dmgReceived,
        int valueKilled,
        int valueLost
    );

    const AccessibilityInfo & getAccessibility() const;
    const Nodes::Unit::Queue & getQueue() const;
    const Nodes::Unit * findUnitByHex(const BattleHex & bh) const;

    const ReachabilityInfo & getReachability(const CStack & cstack) const;
    bool isRUFR(const CStack & cstack, const BattleHex & bh) const;
private:
    // Explicitly building caches allows to define getters as const.
    void cacheAccessibility();
    void cacheQueue(bool isMorale);
    void cacheUnitsByHex();
    void cacheReachability();

    bool haveUnitsByHex = false;
    bool haveReachability = false;

    const CPlayerBattleCallback & battle;
    const CStack * acstack;  // can be nullptr

    GraphStore store;
    std::unordered_map<uint32_t, ReachabilityInfo> rcache;
    std::unordered_map<uint32_t, std::array<bool, GameConstants::BFIELD_SIZE>> rufrHexes;
    std::unique_ptr<AccessibilityInfo> acache;
    std::unique_ptr<Nodes::Unit::Queue> queue;
    std::unordered_map<const BattleHex, const Nodes::Unit &> unitsByHex;
};
}
