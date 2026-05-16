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
#include "BAI/v15/state.h"

#include "schema/v15/graph.h"
#include "schema/v15/types.h"
#include <algorithm>

namespace MMAI::BAI::V15
{
namespace S15 = Schema::V15;
using ET = S15::Graph::ElementType;
using GA = S15::Graph::NodeAttributes::Global;
using PA = S15::Graph::NodeAttributes::Player;
using UA = S15::Graph::NodeAttributes::Unit;
using HA = S15::Graph::NodeAttributes::Hex;
using AA = S15::Graph::NodeAttributes::Action;
namespace EA = S15::Graph::EdgeAttributes;

using INode = S15::Graph::INode;
using IEdge = S15::Graph::IEdge;

// Just for code readability
using IGlobal = INode;
using IPlayer = INode;
using IUnit = INode;
using IHex = INode;
using IAction = INode;

// This function used during model development and is never called otherwise
void Verify(const State * state) // NOSONAR - function used for debugging only
{
    // throw std::runtime_error("not implemented");
    std::cout << "XXX: Verify(): not implemented\n";
}

namespace
{
    std::string PadLeft(const std::string & input, size_t desiredLength, char paddingChar)
    {
        std::ostringstream ss;
        ss << std::right << std::setfill(paddingChar) << std::setw(desiredLength) << input;
        return ss.str();
    }

    template <typename T1, typename T2>
    int attr(const T1 * elem, T2 attr)
    {
        return elem->rawAttributes().at(EU(attr));
    }

    std::map<const IUnit*, int> BuildQueue(const std::vector<const IEdge *> & edges)
    {

        struct OutEdge {
            const IUnit* dst;
            int weight;
        };

        std::map<const IUnit*, std::vector<OutEdge>> graph;
        std::map<const IUnit*, int> indegree;
        std::map<const IUnit*, int> position;

        // Build graph
        for (const auto& edge : edges)
        {
            auto [src, dst] = edge->endpoints();
            const int w = attr(edge, EA::Unit_ActsBefore_Unit::TIMES);

            if (w < 0)
                throw std::runtime_error("ActsBefore::times() cannot be negative");

            graph[src].push_back(OutEdge{.dst = dst, .weight = w});

            indegree.try_emplace(src, 0);
            indegree.try_emplace(dst, 0);
            position.try_emplace(src, 0);
            position.try_emplace(dst, 0);

            ++indegree[dst];
        }

        // Start with nodes that have no incoming constraints
        std::queue<const IUnit*> q;

        for (const auto& [unit, deg] : indegree)
        {
            if (deg == 0) {
                q.push(unit);
            }
        }

        std::size_t visited = 0;

        while (!q.empty())
        {
            const IUnit * src = q.front();
            q.pop();

            ++visited;

            for (const auto& e : graph[src])
            {
                position[e.dst] = std::max(
                    position[e.dst],
                    position[src] + e.weight
                );

                --indegree[e.dst];

                if (indegree[e.dst] == 0) {
                    q.push(e.dst);
                }
            }
        }

        if (visited != indegree.size())
            throw std::runtime_error("ActsBefore graph contains a cycle");

        return position;
    }
}

// This intentionally uses the IState interface to ensure that
// the schema is properly exposing all needed informaton
std::string Render(const Schema::IState * istate, const Action * action) // NOSONAR - function used for debugging only
{
    auto supdata_ = istate->getSupplementaryData();
    ASSERT(supdata_.has_value(), "supdata_ holds no value");
    ASSERT(supdata_.type() == typeid(const S15::ISupplementaryData *), "supdata_ of unexpected type");
    const auto * sup = std::any_cast<const S15::ISupplementaryData *>(supdata_);
    ASSERT(sup, "sup holds a nullptr");
    const auto & alogs = sup->getAttackLogs();
    const auto & G = sup->getGraph();
    const auto * gnode = G->getNodes(ET::NODE_GLOBAL).at(0);
    const auto & pnodes = G->getNodes(ET::NODE_PLAYER);

    const IPlayer * lplayer;
    const IPlayer * rplayer;

    for (const auto & player : pnodes)
    {
        switch(attr(player, PA::BATTLE_SIDE))
        {
        case EU(BattleSide::LEFT_SIDE):
            lplayer = player;
            break;
        case EU(BattleSide::RIGHT_SIDE):
            rplayer = player;
            break;
        default:
            throw std::runtime_error("unexpected player side");
        }
    }

    ASSERT(lplayer && rplayer, "players not found");

    const auto & myplayer = attr(lplayer, PA::IS_ACTIVE) ? lplayer : rplayer;

    ASSERT(attr(myplayer, PA::IS_ACTIVE), "active player not found");

    const IUnit * aunit = nullptr;

    for (const auto & e : G->getEdges(ET::EDGE_ACTION_BY_UNIT))
    {
        aunit = e->endpoints().second;
        break;
    }

    auto ended = attr(gnode, GA::BATTLE_WINNER) != S15::NULL_VALUE_UNENCODED;

    if(!aunit && !ended)
        logAi->error("could not find an active stack (battle has not ended).");

    auto activeActionIds = std::unordered_map<const IAction*, int>{};

    const auto & allActions = G->getNodes(ET::NODE_ACTION);
    for (const auto & [nodeId, actionId] : G->getActiveNodeToActionIds())
    {
        const auto & [_, inserted] = activeActionIds.emplace(allActions.at(nodeId), actionId);
        ASSERT(inserted, "duplicate action"); // should never happen
    }

    // {hex -> [activeAction, ...]}
    auto hexActiveActions = std::unordered_map<const IHex*, std::unordered_set<const IAction*>>{};

    for (const auto & e : G->getEdges(ET::EDGE_ACTION_ENDS_AT_HEX))
    {
        const auto & [action, hex] = e->endpoints();
        if (!activeActionIds.contains(action))
            continue;
        auto [_, inserted] = hexActiveActions[hex].insert(action);
        ASSERT(inserted, "duplicate active action on hex");
    }

    std::string nocol = "\033[0m";
    std::string redcol = "\033[31m"; // red
    std::string bluecol = "\033[34m"; // blue
    std::string darkcol = "\033[90m";
    std::string activemod = "\033[107m\033[7m"; // bold+reversed
    // std::string ukncol = "\033[7m"; // white

    std::vector<std::stringstream> lines;

    //
    // 1. Add logs table:
    //
    // #1 attacks #5 for 16 dmg (1 killed)
    // #5 attacks #1 for 4 dmg (0 killed)
    // ...
    //
    for(const auto & alog : alogs)
    {
        auto row = std::stringstream();
        auto attcol = alog->getAttackerColor();
        auto attalias = alog->getAttackerAlias();
        auto defcol = alog->getDefenderColor();
        auto defalias = alog->getDefenderAlias();

        row << attcol << "#" << attalias << nocol;
        row << " attacks ";
        row << defcol << "#" << defalias << nocol;
        row << " for " << alog->getDamageDealt() << " dmg";
        row << " (kills: " << alog->getUnitsKilled() << ", value: " << alog->getValueKilled() << " / " << alog->getValueKilledPermille() << "‰)";

        lines.push_back(std::move(row));
    }

    //
    // 2. Build ASCII table
    //    (+populate aliveStacks var)
    //    NOTE: the contents below look mis-aligned in some editors.
    //          In (my) terminal, it all looks correct though.
    //
    //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
    //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃
    // ¹┨  1 ◌ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 1 ┠¹
    // ²┨ ◌ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠²
    // ³┨  ◌ ○ ○ ○ ○ ○ ○ ◌ ▦ ▦ ◌ ◌ ◌ ◌ ◌ ┠³
    // ⁴┨ ◌ ○ ○ ○ ○ ○ ○ ○ ▦ ▦ ▦ ◌ ◌ ◌ ◌  ┠⁴
    // ⁵┨  2 ◌ ○ ○ ▦ ▦ ◌ ○ ◌ ◌ ◌ ◌ ◌ ◌ 2 ┠⁵
    // ⁶┨ ◌ ○ ○ ○ ▦ ▦ ◌ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁶
    // ⁷┨  3 3 ○ ○ ○ ▦ ◌ ○ ○ ◌ ◌ ▦ ◌ ◌ 3 ┠⁷
    // ⁸┨ ◌ ○ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌  ┠⁸
    // ⁹┨  ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠⁹
    // ⁰┨ ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁰
    // ¹┨  4 ◌ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 4 ┠¹
    //  ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃
    //   ▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕⁵▕
    //

    // s=special; can be any number, slot is always 7 (SPECIAL), visualized A,B,C...

    auto tablestartrow = lines.size();

    lines.emplace_back() << "    ₀▏₁▏₂▏₃▏₄▏₅▏₆▏₇▏₈▏₉▏₀▏₁▏₂▏₃▏₄";
    lines.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

    static const std::array<std::string, 10> nummap{"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

    bool addspace = true;
    bool divlines = true;

    auto hexstacks = std::unordered_map<const IHex*, const IUnit*>{};
    for (const auto & e : G->getEdges(ET::EDGE_UNIT_OCCUPIES_HEX))
    {
        const auto & [unit, hex] = e->endpoints();
        auto [it, inserted] = hexstacks.try_emplace(hex, unit);
        ASSERT(inserted, "multiple stacks on same hex");
    }
    // y even "▏"
    // y odd "▕"

    // {unit -> hex}
    auto seenunits = std::unordered_map<const IUnit*, const IHex*>{};

    const auto & hexes = G->getNodes(ET::NODE_HEX);
    for(int i = 0; i < hexes.size(); ++i)
    {
        const auto * hex = hexes.at(i);
        const auto * unit = hexstacks[hex]; // nullptr if missing
        auto sym = std::string("?");

        int y = i / 15;
        int x = i % 15;

        const char * spacer = (y % 2 == 0) ? " " : "";
        auto & row = (x == 0) ? (lines.emplace_back() << nummap.at(y % 10) << "┨" << spacer) : lines.back();

        if(addspace)
        {
            if(divlines && (x != 0))
            {
                row << darkcol << (y % 2 == 0 ? "▏" : "▕") << nocol;
            }
            else
            {
                row << " ";
            }
        }

        addspace = true;

        auto smask = S15::HexStateMask(attr(hex, HA::STATE_MASK));
        auto col = nocol;

        // First put symbols based on hex state.
        // If there's a stack on this hex, symbol will be overriden.
        S15::HexStateMask mpass = 1 << EI(S15::HexState::PASSABLE);
        S15::HexStateMask mstop = 1 << EI(S15::HexState::STOPPING);
        S15::HexStateMask mdmgl = 1 << EI(S15::HexState::DAMAGING_L);
        S15::HexStateMask mdmgr = 1 << EI(S15::HexState::DAMAGING_R);
        S15::HexStateMask mdefault = 0; // or mother :)

        std::vector<std::tuple<std::string, std::string, S15::HexStateMask>> symbols{
            {"⨻", bluecol, mpass | mstop | mdmgl},
            {"⨻", redcol,  mpass | mstop | mdmgr},
            {"✶", bluecol, mpass | mdmgl        },
            {"✶", redcol,  mpass | mdmgr        },
            {"△", nocol,   mpass | mstop        },
            {"○", nocol,   mpass                }, // changed to "◌" if unreachable
            {"◼", nocol,   mdefault             }
        };

        for(const auto & tuple : symbols)
        {
            const auto & [s, c, m] = tuple;
            if((smask & m) == m)
            {
                sym = s;
                col = c;
                break;
            }
        }

        auto hasMoveAction = [&hexActiveActions, &activeActionIds](const IHex * hex)
        {
            return std::ranges::find_if(hexActiveActions[hex], [&activeActionIds](const IAction * act) {
                int id = activeActionIds.at(act);
                return (id - 2) % EU(S15::HexAction::_count) == EU(S15::HexAction::MOVE);
            }) != hexActiveActions[hex].end();
        };

        if (col == nocol && ! hasMoveAction(hex))
        { // || supdata->getIsBattleEnded()
            col = darkcol;
            sym = sym == "○" ? "◌" : sym;
        }

        if(unit)
        {
            auto seen = seenunits.contains(unit);
            // MSVC mandates constexpr `n` here

            auto flags = S15::StackFlags1(attr(unit, UA::FLAGS1));
            auto slot = attr(unit, UA::SLOT);
            sym = std::string(1, Graph::Nodes::Unit::CalculateAlias(slot));
            col = attr(unit, UA::SIDE) ? bluecol : redcol;

            if(unit == aunit)
                col += activemod;

            if(flags.test(EU(S15::StackFlag1::IS_WIDE)) && !seen)
            {
                if(attr(unit, UA::SIDE) == 0)
                {
                    sym += "↠";
                    addspace = false;
                }
                else if(attr(unit, UA::SIDE) == 1 && attr(hex, HA::X_COORD) < 14)
                {
                    sym += "↞";
                    addspace = false;
                }
            }

            if(!seen)
                seenunits.try_emplace(unit, hex);
        }

        row << col << sym << nocol;

        if(x == 15 - 1)
        {
            row << (y % 2 == 0 ? " " : "  ") << "┠" << nummap.at(y % 10);
        }
    }

    lines.emplace_back() << " ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃";
    lines.emplace_back() << "   ⁰▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴";

    //
    // 3. Add side table stuff
    //
    //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
    //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃         Player: RED
    // ₁┨  ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠₁    Last action:
    // ₂┨ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠₂      DMG dealt: 0
    // ₃┨  1 ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌ ◌ ◌ 1 ┠₃   Units killed: 0
    // ...

    for(int i = 0; i <= lines.size(); i++)
    {
        std::string name;
        std::string value;
        auto side = attr(gnode, GA::BATTLE_SIDE_ACTIVE_PLAYER);

        switch(i)
        {
            case 1:
                name = "Player";
                if(ended)
                    value = "";
                else
                    value = side ? bluecol + "BLUE" + nocol : redcol + "RED" + nocol;
                break;
            case 2:
                name = "Round";
                value = std::to_string(attr(gnode, GA::BATTLE_ROUND));
                break;
            case 3:
                name = "Last action";
                value = action ? action->name + " [" + std::to_string(action->id) + "]" : "";
                break;
            case 4:
                name = "DMG dealt";
                value = boost::str(boost::format("%d (%d since start)") % attr(myplayer, PA::DMG_DEALT_NOW_ABS) % attr(myplayer, PA::DMG_DEALT_ACC_ABS));
                break;
            case 5:
                name = "DMG received";
                value =
                    boost::str(boost::format("%d (%d since start)") % attr(myplayer, PA::DMG_RECEIVED_NOW_ABS) % attr(myplayer, PA::DMG_RECEIVED_ACC_ABS));
                break;
            case 6:
                name = "Value killed";
                value =
                    boost::str(boost::format("%d (%d since start)") % attr(myplayer, PA::VALUE_KILLED_NOW_ABS) % attr(myplayer, PA::VALUE_KILLED_ACC_ABS));
                break;
            case 7:
                name = "Value lost";
                value = boost::str(boost::format("%d (%d since start)") % attr(myplayer, PA::VALUE_LOST_NOW_ABS) % attr(myplayer, PA::VALUE_LOST_ACC_ABS));
                break;
            case 8:
            {
                // XXX: if there's a draw, this text will be incorrect
                auto restext = attr(gnode, GA::BATTLE_WINNER) ? (bluecol + "BLUE WINS") : (redcol + "RED WINS");

                name = "Battle result";
                value = ended ? (restext + nocol) : "";
            }
            break;
            case 9:
                name = "Army value (L)";
                value = boost::str(
                    boost::format("%d (%.0f‰ of current BF value)") % attr(lplayer, PA::ARMY_VALUE_NOW_ABS) % attr(lplayer, PA::ARMY_VALUE_NOW_REL)
                );
                break;
            case 10:
                name = "Army value (R)";
                value = boost::str(
                    boost::format("%d (%.0f‰ of current BF value)") % attr(rplayer, PA::ARMY_VALUE_NOW_ABS) % attr(rplayer, PA::ARMY_VALUE_NOW_REL)
                );
                break;
            case 11:
                name = "Current BF value";
                value = boost::str(
                    boost::format("%d (%.0f‰ of starting BF value)") % attr(gnode, GA::BFIELD_VALUE_NOW_ABS) % attr(gnode, GA::BFIELD_VALUE_NOW_REL0)
                );
                break;
            default:
                continue;
        }

        lines.at(tablestartrow + i) << PadLeft(name, 17, ' ') << ": " << value;
    }

    lines.emplace_back() << "";

    //
    // 5. Add stacks table:
    //
    //          Stack # |   0   1   2   3   4   5   6   A   B   C   0   1   2   3   4   5   6   A   B   C
    // -----------------+--------------------------------------------------------------------------------
    //              Qty |   0  34   0   0   0   0   0   0   0   0   0  17   0   0   0   0   0   0   0   0
    //           Attack |   0   8   0   0   0   0   0   0   0   0   0   6   0   0   0   0   0   0   0   0
    //    ...10 more... | ...
    // -----------------+--------------------------------------------------------------------------------
    //
    // table with 24 columns (1 header, 3 dividers, 10 stacks per side)
    // Each row represents a separate attribute

    using RowDef = std::tuple<UA, std::string>;

    // max to show
    constexpr int max_stacks_per_side = 10;

    // All cell text is aligned right
    auto colwidths = std::array<int, 4 + (2 * max_stacks_per_side)>{};
    colwidths.fill(5); // default col width
    colwidths.at(0) = 16; // header col

    // Divider column indexes
    auto divcolids = {1, max_stacks_per_side + 2, (2 * max_stacks_per_side) + 3};

    for(int i : divcolids)
        colwidths.at(i) = 2; // divider col

    // {Attribute, name, colwidth}
    const auto rowdefs = std::vector<RowDef>{
        RowDef{UA::FLAGS1,    "Stack #"         }, // stack alias (1..7, S or M)
        RowDef{UA::SIDE,      ""                }, // divider row
        RowDef{UA::QUANTITY,  "Qty"             },
        RowDef{UA::ATTACK,    "Attack"          },
        RowDef{UA::DEFENSE,   "Defense"         },
        RowDef{UA::SHOTS,     "Shots"           },
        RowDef{UA::DMG_MIN,   "Dmg (min)"       },
        RowDef{UA::DMG_MAX,   "Dmg (max)"       },
        RowDef{UA::HP,        "HP"              },
        RowDef{UA::HP_LEFT,   "HP left"         },
        RowDef{UA::SPEED,     "Speed"           },
        RowDef{UA::_count,    "Queue"           }, // no actual enum value for this, use _count...
        RowDef{UA::VALUE_ONE, "Value (one)"     },
        RowDef{UA::VALUE_REL, "       Value (‰)"}, // manually pad to 16 (unicode length issue)
        RowDef{UA::FLAGS1,    "State"           }, // "WAR" = CAN_WAIT, WILL_ACT, CAN_RETAL
        RowDef{UA::FLAGS1,    "Attack mods"     }, // "DB" = Double, Blinding
        // 2 values per column to avoid too long table
        RowDef{UA::FLAGS1,    "Blocked/ing"     },
        RowDef{UA::FLAGS1,    "Fly/Sleep"       },
        RowDef{UA::FLAGS1,    "NoRetal/NoMelee" },
        RowDef{UA::FLAGS1,    "Wide/Breath"     },
        RowDef{UA::SIDE,      ""                }, // divider row
    };

    // Table with nrows and ncells, each cell a 3-element tuple
    // cell: color, width, txt
    using TableCell = std::tuple<std::string, int, std::string>;
    using TableRow = std::array<TableCell, colwidths.size()>;

    auto table = std::vector<TableRow>{};

    auto divrow = TableRow{};
    for(int i = 0; i < colwidths.size(); i++)
        divrow[i] = {nocol, colwidths.at(i), std::string(colwidths.at(i), '-')};

    for(int i : divcolids)
        divrow.at(i) = {nocol, colwidths.at(i), std::string(colwidths.at(i) - 1, '-') + "+"};

    int specialcounter = 0;

    auto queue = BuildQueue(G->getEdges(ET::EDGE_UNIT_ACTS_BEFORE_UNIT));

    auto blockees = std::unordered_map<const IUnit *, const IUnit *>{};
    auto blockers = std::unordered_map<const IUnit *, const IUnit *>{};
    for (const auto & e : G->getEdges(ET::EDGE_UNIT_BLOCKS_UNIT))
    {
        blockers.try_emplace(e->endpoints().second, e->endpoints().first);
        blockers.try_emplace(e->endpoints().first, e->endpoints().second);
    }

    // Attribute rows
    for(const auto & [a, aname] : rowdefs)
    {
        if(a == UA::SIDE)
        { // divider row
            table.push_back(divrow);
            continue;
        }

        auto row = TableRow{};

        // Header col
        row.at(0) = {nocol, colwidths.at(0), aname};

        // Div cols
        for(int i : {1, 2 + max_stacks_per_side, static_cast<int>(colwidths.size() - 1)})
            row.at(i) = {nocol, colwidths.at(i), "|"};

        // Stack cols
        for(auto side : {0, 1})
        {
            auto sideunits = std::array<std::pair<const IUnit *, const IHex *>, max_stacks_per_side>{};
            auto extracounter = 0;
            for(const auto & [unit, hex] : seenunits)
            {
                if(attr(unit, UA::SIDE) == side)
                {
                    char alias = Graph::Nodes::Unit::CalculateAlias(attr(unit, UA::SLOT));
                    int slot = alias >= '0' && alias <= '6' ? alias - '0' : 7 + extracounter;

                    if(slot < max_stacks_per_side)
                        sideunits.at(slot) = {unit, hex};

                    if(slot >= 7)
                        extracounter += 1;
                }
            }

            for(int i = 0; i < sideunits.size(); ++i)
            {
                const auto & [unit, hex] = sideunits.at(i);
                auto colid = 2 + i + side + (max_stacks_per_side * side);

                if(!unit)
                {
                    row.at(colid) = {nocol, colwidths.at(colid), ""};
                    continue;
                }

                std::string value;

                // MSVC mandates constexpr `n` here
                auto flags1 = S15::StackFlags1(attr(unit, UA::FLAGS1));
                auto flags2 = S15::StackFlags2(attr(unit, UA::FLAGS2));
                auto color = attr(unit, UA::SIDE) ? bluecol : redcol;

                if(a == UA::_count) // aka. QUEUE...
                {
                    value = ended ? "" : std::to_string(queue.at(unit));
                }
                else if(a == UA::VALUE_ONE && attr(unit, a) >= 1000)
                {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(1) << (attr(unit, a) / 1000.0);
                    value = oss.str();
                    value[value.size() - 2] = 'k';
                    if(value.rfind("K0") == (value.size() - 2))
                        value.resize(value.size() - 1);
                }
                else if(a == UA::FLAGS1)
                {
                    auto fmt = boost::format("%d/%d");

                    switch(specialcounter)
                    {
                        case 0:
                            value = std::string(1, Graph::Nodes::Unit::CalculateAlias(attr(unit, UA::SLOT)));
                            break;
                        case 1:
                        {
                            value = std::string("");
                            value += flags1.test(EU(S15::StackFlag1::CAN_WAIT)) ? "W" : " ";
                            value += flags1.test(EU(S15::StackFlag1::WILL_ACT)) ? "A" : " ";
                            value += flags1.test(EU(S15::StackFlag1::CAN_RETALIATE)) ? "R" : " ";
                        }
                        break;
                        case 2:
                        {
                            value = std::string("");
                            value += flags1.test(EU(S15::StackFlag1::ADDITIONAL_ATTACK)) ? "D" : " ";
                            value += flags2.test(EU(S15::StackFlag2::BLIND_ATTACK)) ? "B" : " ";
                        }
                        break;
                        case 3:
                            value = boost::str(fmt % blockees.contains(unit) % blockers.contains(unit));
                            break;
                        case 4:
                            value = boost::str(fmt % flags1.test(EU(S15::StackFlag1::FLYING)) % flags1.test(EU(S15::StackFlag1::SLEEPING)));
                            break;
                        case 5:
                            value = boost::str(fmt % flags1.test(EU(S15::StackFlag1::BLOCKS_RETALIATION)) % flags1.test(EU(S15::StackFlag1::NO_MELEE_PENALTY)));
                            break;
                        case 6:
                            value = boost::str(fmt % flags1.test(EU(S15::StackFlag1::IS_WIDE)) % flags1.test(EU(S15::StackFlag1::TWO_HEX_ATTACK_BREATH)));
                            break;
                        default:
                            THROW_FORMAT("Unexpected specialcounter: %d", specialcounter);
                    }
                }
                else
                {
                    value = std::to_string(attr(unit, a));
                }

                if(unit == aunit && !ended)
                    color += activemod;

                row.at(colid) = {color, colwidths.at(colid), value};
            }
        }

        if(a == UA::FLAGS1)
            ++specialcounter;

        table.push_back(row);
    }

    for(const auto & r : table)
    {
        auto line = std::stringstream();
        for(const auto & [color, width, txt] : r)
            line << color << PadLeft(txt, width, ' ') << nocol;

        lines.push_back(std::move(line));
    }

    //
    // 7. Join rows into a single string
    //
    std::string res = lines[0].str();
    for(int i = 1; i < lines.size(); i++)
        res += "\n" + lines[i].str();

    return res;
}
}
