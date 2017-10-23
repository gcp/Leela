#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <cmath>

#include <iostream>
#include <vector>
#include <functional>
#include <algorithm>

#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"
#include "UCTSearch.h"
#include "Utils.h"
#include "Matcher.h"
#include "Network.h"
#include "GTP.h"
#include "Random.h"
#ifdef USE_OPENCL
#include "OpenCL.h"
#endif

using namespace Utils;

UCTNode::UCTNode(int vertex, float score)
    : m_move(vertex), m_score(score) {
}

UCTNode::~UCTNode() {
    LOCK(get_mutex(), lock);
    UCTNode * next = m_firstchild;

    while (next != NULL) {
        UCTNode * tmp = next->m_nextsibling;
        delete next;
        next = tmp;
    }
}

bool UCTNode::first_visit() const {
    return m_visits == 0;
}

void UCTNode::link_child(UCTNode * newchild) {
    newchild->m_nextsibling = m_firstchild;
    m_firstchild = newchild;
}

SMP::Mutex & UCTNode::get_mutex() {
    return m_nodemutex;
}

float UCTNode::create_children(std::atomic<int> & nodecount,
                               GameState & state, bool at_root) {
    // check whether somebody beat us to it (atomic)
    if (has_children()) {
        return;
    }
    // acquire the lock
    LOCK(get_mutex(), lock);
    // no successors in final state
    if (state.get_passes() >= 2) {
        return;
    }
    // check whether somebody beat us to it
    if (at_root && m_has_netscore) {
        return;
    }
    if (m_visits < m_symmetries_done * cfg_extra_symmetry) {
        return;
    }
    if (m_symmetries_done >= 8) {
        assert(!at_root);
        return;
    }
    // Someone else is running the expansion
    if (m_is_expanding) {
        return;
    }
    // Someone else is running an extra netscore
    if (m_is_netscoring) {
        return;
    }
    // We'll be the one queueing this node for expansion, stop others
    m_is_expanding = true;
    m_is_netscoring = true;
    lock.unlock();

    auto raw_netlist = Network::get_Network()->get_scored_moves(
        &state, (at_root ? Network::Ensemble::AVERAGE_ALL :
                           Network::Ensemble::DIRECT), m_symmetries_done);

    // DCNN returns winrate as side to move
    int eval = raw_netlist.second;
    int tomove = state.board.get_to_move();
    if (tomove == FastBoard::WHITE) {
        eval = 1.0f - eval;
    }

    FastBoard & board = state.board;
    std::vector<Network::scored_node> nodelist;

    for (auto& node : raw_netlist.first) {
        auto vertex = node.second;
        if (vertex != FastBoard::PASS) {
            if (vertex != state.m_komove
                && !board.is_suicide(vertex, board.get_to_move())) {
                nodelist.emplace_back(node);
            }
        } else {
            nodelist.emplace_back(node);
        }
    }
    link_nodelist(nodecount, board, nodelist);

    return eval;
}

void UCTNode::link_nodelist(std::atomic<int> & nodecount,
                            FastBoard &board,
                            std::vector<Network::scored_node> & nodelist,
                            bool all_symmetries)
{
    size_t totalchildren = nodelist.size();
    if (!totalchildren)
        return;

    // sort (this will reverse scores, but linking is backwards too)
    std::sort(begin(nodelist), end(nodelist));

    // link the nodes together, we only really link the last few
    size_t maxchilds = 35;
    int childrenadded = 0;
    size_t childrenseen = 0;

    LOCK(get_mutex(), lock);

    for (const auto& node : nodelist) {
        if (totalchildren - childrenseen <= maxchilds) {
            UCTNode *vtx = new UCTNode(node.second, node.first);
            link_child(vtx);
            childrenadded++;
        }
        childrenseen++;
    }

    nodecount += childrenadded;
    m_has_children = true;
    m_has_netscore = true;
    m_is_netscoring = false;
    if (all_symmetries) {
        m_symmetries_done = 8;
    } else {
        m_symmetries_done++;
    }
}

void UCTNode::rescore_nodelist(std::atomic<int> & nodecount,
                               FastBoard & board,
                               std::vector<Network::scored_node> & nodelist,
                               bool all_symmetries) {

    assert(!nodelist.empty());
    // sort (this will reverse scores, but linking is backwards too)
    std::sort(begin(nodelist), end(nodelist));

    int childrenadded = 0;
    const int max_net_childs = 35;

    LOCK(get_mutex(), lock);

    for (const auto& node : nodelist) {
        // Check for duplicate moves, O(N^2)
        bool found = false;
        UCTNode * child = m_firstchild;
        while (child != nullptr) {
            if (child->get_move() == it->second) {
                found = true;
                break;
            }
            child = child->m_nextsibling;
        }
        if (!found) {
            // Not added yet, is it highly scored?
            if (std::distance(it, nodelist.cend()) <= max_net_childs) {
                UCTNode * vtx = new UCTNode(node.second, node.first);
                link_child(vtx);
                childrenadded++;
            }
        } else {
            // Average_all run at the root
            if (all_symmetries) {
                child->set_score(node.first);
            } else {
                assert(m_symmetries_done > 0 && m_symmetries_done < 8);
                float oldscore = child->get_score();
                float factor = 1.0f / ((float)m_symmetries_done + 1.0f);
                child->set_score(node.first * factor
                                 + oldscore * (1.0f - factor));
            }
        }
    }

    nodecount += childrenadded;
    sort_children();
    m_has_children = true;
    m_has_netscore = true;
    m_is_netscoring = false;
    if (all_symmetries) {
        m_symmetries_done = 8;
    } else {
        m_symmetries_done++;
    }
}

void UCTNode::kill_superkos(KoState & state) {
    UCTNode * child = m_firstchild;

    while (child != nullptr) {
        int move = child->get_move();

        if (move != FastBoard::PASS) {
            KoState mystate = state;
            mystate.play_move(move);

            if (mystate.superko()) {
                UCTNode * tmp = child->m_nextsibling;
                delete_child(child);
                child = tmp;
                continue;
            }
        }
        child = child->m_nextsibling;
    }
}

int UCTNode::get_move() const {
    return m_move;
}

void UCTNode::set_move(int move) {
    m_move = move;
}

void UCTNode::update(int color, bool update_eval, float eval) {
    m_visits++;

    // evals
    if (update_eval) {
        accumulate_eval(eval);
    }
}

bool UCTNode::has_children() const {
    return m_has_children;
}

// XXX unneeded
bool UCTNode::has_netscore() const {
    return m_has_netscore;
}

void UCTNode::set_visits(int visits) {
    m_visits = visits;
}

float UCTNode::get_score() const {
    return m_score;
}

void UCTNode::set_score(float score) {
    m_score = score;
}

int UCTNode::get_visits() const {
    return m_visits;
}

float UCTNode::get_eval(int tomove) const {
    float score = m_blackevals / (double)m_evalcount;
    if (tomove == FastBoard::WHITE) {
        score = 1.0f - score;
    }
    return score;
}

double UCTNode::get_blackevals() const {
    return m_blackevals;
}

void UCTNode::set_blackevals(double blackevals) {
    m_blackevals = blackevals;
}

void UCTNode::set_evalcount(int evalcount) {
    m_evalcount = evalcount;
}

int UCTNode::get_evalcount() const {
    return m_evalcount;
}

void UCTNode::accumulate_eval(float eval) {
    atomic_add(m_blackevals, (double)eval);
    m_evalcount  += 1;
}

float UCTNode::smp_noise(void) {
    if (cfg_num_threads >= 6) {
        float winnoise = 0.0025f;
        if (cfg_num_threads >= 12) {
            winnoise = 0.04f;
        }
        return winnoise * Random::get_Rng()->randflt();
    } else {
        return 0.0f;
    }
}

UCTNode* UCTNode::uct_select_child(int color, bool use_nets) {
    UCTNode * best = NULL;
    float best_value = -1000.0f;
    int childbound;
    int parentvisits = 1; // XXX: this can be 0 now that we sqrt
    float best_probability = 0.0f;

    LOCK(get_mutex(), lock);
    if (has_netscore()) {
        childbound = 35;
    } else {
        if (use_nets) {
            childbound = cfg_rave_moves;
        } else {
            childbound = std::max(2, (int)(((log((double)get_visits()) - 3.0) * 3.0) + 2.0));
        }
    }

    int childcount = 0;
    UCTNode * child = m_firstchild;

    // count parentvisits
    // XXX: wtf do we count this??? don't we know?
    // make sure we are at a valid successor
    while (child != NULL && !child->valid()) {
        child = child->m_nextsibling;
    }
    while (child != NULL && childcount < childbound) {
        parentvisits      += child->get_visits();
        child = child->m_nextsibling;
        // make sure we are at a valid successor
        while (child != NULL && !child->valid()) {
            child = child->m_nextsibling;
        }
        childcount++;
    }
    float numerator = std::log((float)parentvisits);

    float cutoff_ratio;
    childcount = 0;
    child = m_firstchild;
    // make sure we are at a valid successor
    while (child != NULL && !child->valid()) {
        child = child->m_nextsibling;
    }
    if (has_netscore()) {
        // first move
        if (child != NULL) {
            best_probability = child->get_score();
        }
        assert(best_probability > 0.001f);
        cutoff_ratio = cfg_cutoff_offset + cfg_cutoff_ratio * numerator;
    }
    while (child != NULL && childcount < childbound) {
        float value;

        if (has_netscore()) {
            if (child->get_score() * cutoff_ratio < best_probability) {
                break;
            }

            if (!child->first_visit()) {
                // "UCT" part
                float winrate = child->get_mixed_score(color);
                winrate += smp_noise();
                float psa = child->get_score();
                float denom = 1.0f + child->get_visits();

                float mti = (cfg_psa / psa) * std::sqrt(numerator / parentvisits);
                float puct = cfg_puct * psa * ((float)std::sqrt(parentvisits) / denom);
                // float cts = cfg_puct * std::sqrt(numerator / denom);
                // Alternate is to remove psa in puct but without log(parentvis)

                value = winrate - mti + puct;
            } else {
                float winrate = cfg_fpu;
                winrate += smp_noise();
                float psa = child->get_score();
                float mti;
                if (parentvisits > 1) {
                    mti = (cfg_psa / psa) * std::sqrt(numerator / parentvisits);
                } else {
                    mti = (cfg_psa / psa);
                }

                value = winrate - mti + cfg_puct * psa * (float)std::sqrt(parentvisits);
                assert(value > -1000.0f);
            }
        } else {
            float uctvalue;
            float patternbonus;
            assert(child->get_ravevisits() > 0);
            if (!child->first_visit()) {
                // "UCT" part
                float winrate = child->get_mixed_score(color);
                winrate += smp_noise();
                uctvalue = winrate + cfg_uct * std::sqrt(numerator / child->get_visits());
                patternbonus = sqrtf((child->get_score() * cfg_patternbonus) / child->get_visits());
            } else {
                uctvalue = 1.1f;
                patternbonus = sqrtf(child->get_score() * cfg_patternbonus);
            }

            // RAVE part
            float ravewinrate = child->get_raverate();
            float ravevalue = ravewinrate + patternbonus;
            float beta = std::max(0.0, 1.0 - log(1.0 + child->get_visits()) / cfg_beta);

            value = beta * ravevalue + (1.0f - beta) * uctvalue;
            assert(value > -1000.0f);
        }
        assert(value > -1000.0f);

        if (value > best_value) {
            best_value = value;
            best = child;
        }

        child = child->m_nextsibling;
        // make sure we are at a valid successor
        while (child != NULL && !child->valid()) {
            child = child->m_nextsibling;
        }
        childcount++;
    }

    assert(best != NULL);

    return best;
}

class NodeComp : public std::binary_function<UCTNode::sortnode_t, UCTNode::sortnode_t, bool> {
private:
    const int m_maxvisits;
public:
    NodeComp(const int maxvisits) : m_maxvisits(maxvisits) {}

    bool operator()(const UCTNode::sortnode_t a, const UCTNode::sortnode_t b) {
        // edge cases, one playout or none
        if (!std::get<1>(a) && std::get<1>(b)) {
            return false;
        }

        if (!std::get<1>(b) && std::get<1>(a)) {
            return true;
        }

        if (!std::get<1>(a) && !std::get<1>(b)) {
            if ((std::get<2>(a))->get_score() > (std::get<2>(b))->get_score()) {
                return true;
            } else {
                return false;
            }
        }

        // first check: are playouts comparable and sufficient?
        // then winrate counts
        if (std::get<1>(a) > cfg_mature_threshold
            && std::get<1>(b) > cfg_mature_threshold
            && std::get<1>(a) * 2 > m_maxvisits
            && std::get<1>(b) * 2 > m_maxvisits) {

            if (std::get<0>(a) == std::get<0>(b)) {
                if (std::get<1>(a) > std::get<1>(b)) {
                    return true;
                } else {
                    return false;
                }
            } else if (std::get<0>(a) > std::get<0>(b)) {
                return true;
            } else {
                return false;
            }
        } else {
            // playout amount differs greatly, prefer playouts
            if (std::get<1>(a) > std::get<1>(b)) {
                return true;
            } else {
                return false;
            }
        }
    }
};

/*
    sort children by converting linked list to vector,
    sorting the vector, and reconstructing to linked list again
    Requires node mutex to be held.
*/
void UCTNode::sort_children() {
    assert(get_mutex().is_held());
    std::vector<std::tuple<float, UCTNode*>> tmp;

    UCTNode * child = m_firstchild;

    while (child != nullptr) {
        tmp.emplace_back(child->get_score(), child);
        child = child->m_nextsibling;
    }

    std::sort(begin(tmp.), end(tmp));

    m_firstchild = nullptr;

    for (auto& sortnode : tmp) {
        link_child(std::get<1>(tmp));
    }
}

void UCTNode::sort_root_children(int color) {
    LOCK(get_mutex(), lock);
    std::vector<sortnode_t> tmp;

    UCTNode * child = m_firstchild;
    int maxvisits = 0;

    while (child != nullptr) {
        int visits = child->get_visits();
        if (visits) {
            float winrate = child->get_mixed_score(color);
            tmp.emplace_back(winrate, visits, child);
        } else {
            tmp.emplace_back(0.0f, 0, child);
        }
        maxvisits = std::max(maxvisits, visits);
        child = child->m_nextsibling;
    }

    // reverse sort, because list reconstruction is backwards
    std::stable_sort(tmp.rbegin(), tmp.rend(), NodeComp(maxvisits));

    m_firstchild = nullptr;

    for (auto& sortnode : tmp) {
        link_child(std::get<2>(sortnode));
    }
}

UCTNode* UCTNode::get_first_child() const {
    return m_firstchild;
}

UCTNode* UCTNode::get_sibling() const {
    return m_nextsibling;
}

UCTNode* UCTNode::get_pass_child() const {
    UCTNode * child = m_firstchild;

    while (child != nullptr) {
        if (child->m_move == FastBoard::PASS) {
            return child;
        }
        child = child->m_nextsibling;
    }

    return nullptr;
}

UCTNode* UCTNode::get_nopass_child() const {
    UCTNode * child = m_firstchild;

    while (child != nullptr) {
        if (child->m_move != FastBoard::PASS) {
            return child;
        }
        child = child->m_nextsibling;
    }

    return nullptr;
}

void UCTNode::invalidate() {
    m_valid = false;
}

bool UCTNode::valid() const {
    return m_valid;
}

// unsafe in SMP, we don't know if people hold pointers to the
// child which they might dereference
void UCTNode::delete_child(UCTNode * del_child) {
    LOCK(get_mutex(), lock);
    assert(del_child != nullptr);

    if (del_child == m_firstchild) {
        m_firstchild = m_firstchild->m_nextsibling;
        delete del_child;
        return;
    } else {
        UCTNode * child = m_firstchild;
        UCTNode * prev  = nullptr;

        do {
            prev  = child;
            child = child->m_nextsibling;

            if (child == del_child) {
                prev->m_nextsibling = child->m_nextsibling;
                delete del_child;
                return;
            }
        } while (child != nullptr);
    }

    assert(false && "Child to delete not found");
}
