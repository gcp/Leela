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

UCTNode::UCTNode(int vertex, float score, int expand_threshold,
                 int netscore_threshold, int movenum)
    : m_firstchild(nullptr), m_nextsibling(nullptr),
      m_move(vertex), m_blackwins(0.0), m_visits(0), m_score(score),
      m_eval_propagated(false), m_blackevals(0.0f),
      m_evalcount(0), m_movenum(movenum), m_is_evaluating(false),
      m_valid(true), m_expand_cnt(expand_threshold), m_is_expanding(false),
      m_has_netscore(false), m_netscore_thresh(netscore_threshold),
      m_symmetries_done(0), m_is_netscoring(false) {
    m_ravevisits = 20;
    m_ravestmwins = 10.0;
}

UCTNode::~UCTNode() {
    SMP::Lock lock(get_mutex());
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

bool UCTNode::should_expand() const {
    return m_visits > m_expand_cnt;
}

bool UCTNode::should_netscore() const {
    return m_visits > m_netscore_thresh;
}

void UCTNode::link_child(UCTNode * newchild) {
    newchild->m_nextsibling = m_firstchild;
    m_firstchild = newchild;
}

SMP::Mutex & UCTNode::get_mutex() {
    return m_nodemutex;
}

void UCTNode::netscore_children(std::atomic<int> & nodecount,
                                FastState & state, bool at_root) {
    // acquire the lock
    SMP::Lock lock(get_mutex());
    // check whether somebody beat us to it
    if (at_root && m_has_netscore) {
        return;
    }
    if (m_symmetries_done >= 8) {
        assert(!at_root);
        return;
    }
    if (m_visits < m_symmetries_done * cfg_extra_symmetry) {
        return;
    }
#ifdef USE_OPENCL
    // Previous kernel is still running, skip this expansion for now
    if (!opencl.thread_can_issue()) {
        // We don't abort them when the search ends
        // assert(!at_root);
        return;
    }
#endif
    // Someone else is running the expansion
    if (m_is_netscoring) {
        return;
    }
    // We'll be the one queueing this node for expansion, stop others
    m_is_netscoring = true;
    // Let simulations proceed
    lock.unlock();

#ifdef USE_OPENCL
    if (at_root) {
        auto raw_netlist = Network::get_Network()->get_scored_moves(
            &state, Network::Ensemble::AVERAGE_ALL);
        scoring_cb(&nodecount, state, raw_netlist, at_root);
    } else {
        Network::get_Network()->async_scored_moves(
            &nodecount, &state, this, Network::Ensemble::DIRECT, m_symmetries_done);
    }
#else
    auto raw_netlist = Network::get_Network()->get_scored_moves(
        &state, (at_root ? Network::Ensemble::AVERAGE_ALL :
                           Network::Ensemble::DIRECT), m_symmetries_done);
    scoring_cb(&nodecount, state, raw_netlist, at_root);
#endif
}

void UCTNode::create_children(std::atomic<int> & nodecount,
                              FastState & state, bool at_root, bool use_nets) {
    // acquire the lock
    SMP::Lock lock(get_mutex());
    // check whether somebody beat us to it
    if (has_children()) {
        return;
    }
    // no successors in final state
    if (state.get_passes() >= 2) {
        return;
    }
    // Someone else is running the expansion
    if (m_is_expanding) {
        return;
    }
    // We'll be the one queueing this node for expansion, stop others
    m_is_expanding = true;
    lock.unlock();

    FastBoard & board = state.board;
    std::vector<Network::scored_node> nodelist;

    std::vector<int> territory = state.board.influence();
    std::vector<int> moyo = state.board.moyo();

    for (int i = 0; i < board.get_empty(); i++) {
        int vertex = board.get_empty_vertex(i);
        assert(board.get_square(vertex) == FastBoard::EMPTY);
        // add and score a node
        if (vertex != state.m_komove && board.no_eye_fill(vertex)) {
            if (!board.is_suicide(vertex, board.get_to_move())) {
                float score = state.score_move(territory, moyo, vertex);
                nodelist.push_back(std::make_pair(score, vertex));
            }
        }
    }
    float passscore;
    if (at_root) {
        passscore = state.score_move(territory, moyo, FastBoard::PASS);
    } else {
        passscore = 0;
    }
    nodelist.push_back(std::make_pair(passscore, +FastBoard::PASS));
    link_nodelist(nodecount, board, nodelist, use_nets);
}

void UCTNode::scoring_cb(std::atomic<int> * nodecount,
                         FastState & state,
                         Network::Netresult & raw_netlist,
                         bool all_symmetries) {
    FastBoard & board = state.board;
    std::vector<Network::scored_node> nodelist;

    for (auto it = raw_netlist.begin(); it != raw_netlist.end(); ++it) {
        int vertex = it->second;
        if (vertex != state.m_komove && board.no_eye_fill(vertex)) {
            if (!board.is_suicide(vertex, board.get_to_move())) {
                nodelist.push_back(*it);
            }
        }
    }
    nodelist.push_back(std::make_pair(0.0f, +FastBoard::PASS));
    rescore_nodelist(*nodecount, board, nodelist, all_symmetries);
}

void UCTNode::rescore_nodelist(std::atomic<int> & nodecount,
                               FastBoard & board,
                               Network::Netresult & nodelist,
                               bool all_symmetries) {

    assert(!nodelist.empty());
    // sort (this will reverse scores, but linking is backwards too)
    std::sort(nodelist.begin(), nodelist.end());

    int childrenadded = 0;
    const int max_net_childs = 35;
    int netscore_threshold = cfg_mature_threshold;
    int expand_threshold = ((float)cfg_mature_threshold)/cfg_expand_divider;
    int movenum = board.get_stone_count();

    SMP::Lock lock(get_mutex());

    for (auto it = nodelist.cbegin(); it != nodelist.cend(); ++it) {
        // Check for duplicate moves, O(N^2)
        bool found = false;
        UCTNode * child = m_firstchild;
        while (child != NULL) {
            if (child->get_move() == it->second) {
                found = true;
                break;
            }
            child = child->m_nextsibling;
        }
        if (!found) {
            // Not added yet, is it highly scored?
            if (std::distance(it, nodelist.cend()) <= max_net_childs) {
                UCTNode * vtx = new UCTNode(it->second, it->first,
                                            expand_threshold, netscore_threshold,
                                            movenum);
                if (it->second != FastBoard::PASS) {
                    // atari giving
                    // was == 2, == 1
                    if (board.minimum_elib_count(board.get_to_move(), it->second) <= 2) {
                        vtx->set_expand_cnt(expand_threshold / 3, netscore_threshold / 3);
                    }
                    if (board.minimum_elib_count(!board.get_to_move(), it->second) == 1) {
                        vtx->set_expand_cnt(expand_threshold / 3, netscore_threshold / 3);
                    }
                }
                link_child(vtx);
                childrenadded++;
            }
        } else {
            // Found
            // First net run, or average_all run at the root
            // Overwrite MC score with netscore
            if (m_symmetries_done == 0 || all_symmetries) {
                child->set_score(it->first);
            } else {
                assert(m_symmetries_done > 0 && m_symmetries_done < 8);
                float oldscore = child->get_score();
                float factor = 1.0f / ((float)m_symmetries_done + 1.0f);
                child->set_score(it->first * factor
                                 + oldscore * (1.0f - factor));
            }
        }
    }

    nodecount += childrenadded;
    sort_children();
    m_has_netscore = true;
    m_is_netscoring = false;
    if (all_symmetries) {
        m_symmetries_done = 8;
    } else {
        m_symmetries_done++;
    }
}

void UCTNode::link_nodelist(std::atomic<int> & nodecount,
                            FastBoard & board,
                            Network::Netresult & nodelist,
                            bool use_nets) {
    int totalchildren = nodelist.size();
    if (!totalchildren) return;

    // sort (this will reverse scores, but linking is backwards too)
    std::sort(nodelist.begin(), nodelist.end());

    // link the nodes together, we only really link the last few
    int maxchilds = 35;     // about 35 -> 4M visits
    if (use_nets) {
        maxchilds = 15;     // XXX: run formula but can't be much
    }
    int childrenadded = 0;
    int childrenseen = 0;

    int netscore_threshold = cfg_mature_threshold;
    if (!use_nets) {
        if (board.get_boardsize() <= 9) {
            netscore_threshold = 30;
        } else {
            netscore_threshold = 50;
        }
    }
    int expand_threshold = ((float)netscore_threshold)/cfg_expand_divider;
    int movenum = board.get_stone_count();

    SMP::Lock lock(get_mutex());

    for (auto it = nodelist.cbegin(); it != nodelist.cend(); ++it) {
        if (totalchildren - childrenseen <= maxchilds) {
            UCTNode * vtx = new UCTNode(it->second, it->first,
                                        expand_threshold, netscore_threshold,
                                        movenum);
            if (it->second != FastBoard::PASS) {
                // atari giving
                // was == 2, == 1
                if (board.minimum_elib_count(board.get_to_move(), it->second) <= 2) {
                    vtx->set_expand_cnt(expand_threshold / 3, netscore_threshold / 3);
                }
                if (board.minimum_elib_count(!board.get_to_move(), it->second) == 2) {
                    vtx->set_expand_cnt(expand_threshold / 2, netscore_threshold / 2);
                }
                if (board.minimum_elib_count(!board.get_to_move(), it->second) == 1) {
                    vtx->set_expand_cnt(expand_threshold / 3, netscore_threshold / 3);
                }
            }
            link_child(vtx);
            childrenadded++;
        }
        childrenseen++;
    }

    nodecount += childrenadded;
}

void UCTNode::run_value_net(FastState & state) {
    // acquire the lock
    SMP::Lock lock(get_mutex());
    // check whether somebody beat us to it
    if (get_evalcount()) {
        return;
    }
    if (m_is_evaluating) {
        return;
    }
    assert(!has_eval_propagated());

    // We'll be the one evaluating this node, stop others
    m_is_evaluating = true;
    // Let simulations proceed
    lock.unlock();

    float eval =
        Network::get_Network()->get_value(&state,
                                          Network::Ensemble::RANDOM_ROTATION);

    // DCNN returns winrate as side to move
    int tomove = state.board.get_to_move();
    if (tomove == FastBoard::WHITE) {
        eval = 1.0f - eval;
    }
    lock.lock();
    accumulate_eval(eval);
}

void UCTNode::kill_superkos(KoState & state) {        
    UCTNode * child = m_firstchild;
    
    while (child != NULL) {
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

void UCTNode::set_expand_cnt(int runs, int netscore_cnt) {
    m_expand_cnt = runs;
    m_netscore_thresh = netscore_cnt;
}

void UCTNode::update(Playout & gameresult, int color, bool update_eval) {
    m_visits++;
    m_ravevisits++;

    // prefer winning with more territory
    float score = gameresult.get_score();
    double blackwins_inc = 0.05 * score;
    if (score > 0.0f) {
        blackwins_inc += 1.0;
    } else if (score == 0.0f) {
        blackwins_inc += 0.5;
    }
    atomic_add(m_blackwins, blackwins_inc);

    // We're inspected from one level above and scores
    // are side to move, so invert here
    if (color == FastBoard::BLACK) {
        if (score < 0.0f) {
            atomic_add(m_ravestmwins, 1.0 + 0.05 * -score);
        }
    } else if (color == FastBoard::WHITE) {
        if (score > 0.0f) {
            atomic_add(m_ravestmwins, 1.0 + 0.05 * score);
        }
    }

    // evals
    if (gameresult.has_eval() && update_eval) {
        accumulate_eval(gameresult.get_eval());
    }
}

bool UCTNode::has_children() const {
    return m_firstchild != NULL;
}

double UCTNode::get_blackwins() const {
    return m_blackwins;
}

void UCTNode::set_visits(int visits) {
    m_visits = visits;
}

void UCTNode::set_blackwins(double wins) {
    m_blackwins = wins;
}

float UCTNode::get_score() const {
    return m_score;
}

void UCTNode::set_score(float score) {
    m_score = score;
}

float UCTNode::get_winrate(int tomove) const {    
    assert(!first_visit());

    float rate = get_blackwins() / get_visits();

    if (tomove == FastBoard::WHITE) {
        rate = 1.0f - rate;
    }

    return rate;
}

float UCTNode::get_raverate() const {
    float rate = m_ravestmwins / m_ravevisits;

    return rate;
}

int UCTNode::get_visits() const {
    return m_visits;
}

int UCTNode::get_ravevisits() const {
    return m_ravevisits;
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
    // Set from TT. We don't need to re-eval if from hash.
    if (evalcount) {
        set_eval_propagated();
    }
}

int UCTNode::get_evalcount() const {
    return m_evalcount;
}

bool UCTNode::has_eval_propagated() const {
    return m_eval_propagated;
}

void UCTNode::set_eval_propagated() {
    m_eval_propagated = true;
}

void UCTNode::accumulate_eval(float eval) {
    atomic_add(m_blackevals, (double)eval);
    m_evalcount  += 1;
}

float UCTNode::score_mix_function(int movenum, float eval, float winrate) {
    float opening_mix = eval * cfg_mix_opening + winrate * (1.0f - cfg_mix_opening);
    float ending_mix = eval * cfg_mix_ending + winrate * (1.0f - cfg_mix_ending);
    if (movenum > 200) {
        return ending_mix;
    }
    float ratio = movenum / 200.0f;
    return opening_mix * (1.0f - ratio) + ending_mix * ratio;
}

float UCTNode::get_mixed_score(int tomove) {
    if (first_visit()) {
        return 0.0f;
    }
    float winrate = get_winrate(tomove);
    int evalcount = get_evalcount();
    if (!evalcount) {
        return winrate;
    }
    float eval = get_eval(tomove);
    return UCTNode::score_mix_function(m_movenum, eval, winrate);
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
    if (has_netscore()) {
        childbound = 35;
    } else {
        if (use_nets) {
            childbound = cfg_rave_moves;
        } else {
            childbound = std::max(2, (int)(((log((double)get_visits()) - 3.0) * 3.0) + 2.0));
        }
    }
    SMP::Lock lock(get_mutex());

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
                float puct = cfg_puct * psa * (std::sqrt(parentvisits) / denom);
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

                value = winrate - mti + cfg_puct * psa * std::sqrt(parentvisits);
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
                uctvalue = cfg_mcts_fpu;
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

    while (child != NULL) {
        tmp.push_back(std::make_tuple(child->get_score(), child));
        child = child->m_nextsibling;
    }

    std::sort(tmp.begin(), tmp.end());

    m_firstchild = NULL;

    for (auto it = tmp.begin(); it != tmp.end(); ++it) {
        link_child(std::get<1>(*it));
    }
}

void UCTNode::sort_root_children(int color) {
    SMP::Lock lock(get_mutex());
    std::vector<sortnode_t> tmp;

    UCTNode * child = m_firstchild;
    int maxvisits = 0;

    while (child != NULL) {
        int visits = child->get_visits();
        if (visits) {
            float winrate = child->get_mixed_score(color);
            tmp.push_back(std::make_tuple(winrate, visits, child));
        } else {
            tmp.push_back(std::make_tuple(0.0f, 0, child));
        }
        maxvisits = std::max(maxvisits, visits);
        child = child->m_nextsibling;
    }

    // reverse sort, because list reconstruction is backwards
    std::stable_sort(tmp.rbegin(), tmp.rend(), NodeComp(maxvisits));

    m_firstchild = NULL;

    for (auto it = tmp.begin(); it != tmp.end(); ++it) {
        link_child(std::get<2>(*it));
    }
}

UCTNode* UCTNode::get_first_child() {
    return m_firstchild;
}

UCTNode* UCTNode::get_sibling() {
    return m_nextsibling;
}

UCTNode* UCTNode::get_pass_child() {
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        if (child->m_move == FastBoard::PASS) {
            return child;
        }
                        
        child = child->m_nextsibling;       
    }              
    
    return NULL;  
}

UCTNode* UCTNode::get_nopass_child() {
    UCTNode * child = m_firstchild;    
    
    while (child != NULL) {        
        if (child->m_move != FastBoard::PASS) {
            return child;
        }
                        
        child = child->m_nextsibling;       
    }              
    
    return NULL;  
}

void UCTNode::invalidate() {
    SMP::Lock lock(get_mutex());
    m_valid = false;
}

bool UCTNode::valid() {
    return m_valid;
}

// unsafe in SMP, we don't know if people hold pointers to the 
// child which they might dereference
void UCTNode::delete_child(UCTNode * del_child) {  
    SMP::Lock lock(get_mutex());     
    assert(del_child != NULL);
    
    if (del_child == m_firstchild) {           
        m_firstchild = m_firstchild->m_nextsibling; 
        delete del_child;       
        return;
    } else {
        UCTNode * child = m_firstchild;    
        UCTNode * prev  = NULL;
    
        do {
            prev  = child;            
            child = child->m_nextsibling;
            
            if (child == del_child) {                
                prev->m_nextsibling = child->m_nextsibling;
                delete del_child;
                return;
            }                                    
        } while (child != NULL);     
    }         

    assert(0 && "Child to delete not found");           
}

// update siblings with matching RAVE info
void UCTNode::updateRAVE(Playout & playout, int color) {
    float score = playout.get_score();

    // siblings
    UCTNode * child = m_firstchild;

    while (child != NULL) {
        int move = child->get_move();

        if (color == FastBoard::BLACK) {
            bool bpass = playout.passthrough(FastBoard::BLACK, move);

            if (bpass) {
                child->m_ravevisits++;

                if (score > 0.0f) {
                    atomic_add(child->m_ravestmwins, 1.0 + 0.05 * score);
                } else if (score == 0.0f) {
                    atomic_add(child->m_ravestmwins, 0.5);
                }
            }
        } else {
            bool wpass = playout.passthrough(FastBoard::WHITE, move);

            if (wpass) {
                child->m_ravevisits++;

                if (score < 0.0f) {
                    atomic_add(child->m_ravestmwins, 1.0 + 0.05 * -score);
                } else if (score == 0.0f) {
                    atomic_add(child->m_ravestmwins, 0.5);
                }
            }
        }

        child = child->m_nextsibling;
    }
}
