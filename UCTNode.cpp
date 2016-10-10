#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include <vector>
#include <functional>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

#include "FastState.h"
#include "Playout.h"
#include "UCTNode.h"
#include "UCTSearch.h"
#include "Utils.h"
#include "Matcher.h"
#include "Network.h"
#include "GTP.h"
#ifdef USE_OPENCL
#include "OpenCL.h"
#endif

using namespace Utils;

UCTNode::UCTNode(int vertex, float score, int expand_treshold)
    : m_firstchild(NULL), m_move(vertex), m_blackwins(0.0), m_visits(0), m_score(score),
      m_valid(true), m_expand_cnt(expand_treshold), m_is_expanding(false) {
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

void UCTNode::link_child(UCTNode * newchild) {
    newchild->m_nextsibling = m_firstchild;
    m_firstchild = newchild;
}

SMP::Mutex & UCTNode::get_mutex() {
    return m_nodemutex;
}

void UCTNode::create_children(boost::atomic<int> & nodecount,
                              FastState & state, bool use_nets, bool at_root) {
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
#ifdef USE_OPENCL
    // Previous kernel is still running, skip this expansion for now
    if (!OpenCL::get_OpenCL()->thread_can_issue()) {
        // We don't abort them when the search ends
        // assert(!at_root);
        return;
    }
#endif
    // Someone else is running the expansion
    if (m_is_expanding) {
        return;
    }
    // We'll be the one queueing this node for expansion, stop others
    m_is_expanding = true;
    lock.unlock();

    FastBoard & board = state.board;
    std::vector<Network::scored_node> nodelist;

    if (use_nets) {
#ifdef USE_OPENCL
        if (at_root) {
           auto raw_netlist = Network::get_Network()->get_scored_moves(
               &state, Network::Ensemble::AVERAGE_ALL);
           expansion_cb(&nodecount, state, raw_netlist);
        } else {
            Network::get_Network()->async_scored_moves(
                &nodecount, &state, this, Network::Ensemble::RANDOM_ROTATION);
        }
#else
        std::vector<Network::scored_node> raw_netlist;
        if (state.get_passes() < 2) {
            raw_netlist = Network::get_Network()->get_scored_moves(
              &state, (at_root ? Network::Ensemble::AVERAGE_ALL :
                                 Network::Ensemble::RANDOM_ROTATION));
            for (auto it = raw_netlist.begin(); it != raw_netlist.end(); ++it) {
                int vertex = it->second;
                if (vertex != state.m_komove && board.no_eye_fill(vertex)) {
                    if (!board.is_suicide(vertex, board.get_to_move())) {
                        nodelist.push_back(*it);
                    }
                }
            }
            nodelist.push_back(std::make_pair(0.0f, +FastBoard::PASS));
        }
        link_nodelist(nodecount, board, nodelist, use_nets);
#endif
    } else {
        std::vector<int> territory = state.board.influence();
        std::vector<int> moyo = state.board.moyo();

        if (state.get_passes() < 2) {
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
    }
}

#ifdef USE_OPENCL
void UCTNode::expansion_cb(boost::atomic<int> * nodecount,
                           FastState & state,
                           std::vector<Network::scored_node> & raw_netlist) {
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

    link_nodelist(*nodecount, board, nodelist, true);
}
#endif

void UCTNode::link_nodelist(boost::atomic<int> & nodecount,
                            FastBoard & board,
                            std::vector<Network::scored_node> & nodelist,
                            bool use_nets) {

    // sort (this will reverse scores, but linking is backwards too)
    std::stable_sort(nodelist.begin(), nodelist.end());

    // link the nodes together, we only really link the last few
    const int maxchilds = 35;   // about 35 -> 4M visits
    int childrenadded = 0;
    int childrenseen = 0;
    int totalchildren = nodelist.size();
    if (!totalchildren) return;

    int expand_treshold = UCTSearch::MCTS_MATURE_TRESHOLD;
    if (use_nets) {
        expand_treshold = cfg_mcnn_maturity; //UCTSearch::MCNN_MATURE_TRESHOLD;
    }

    SMP::Lock lock(get_mutex());

    for (auto it = nodelist.cbegin(); it != nodelist.cend(); ++it) {
        if (totalchildren - childrenseen <= maxchilds) {
            UCTNode * vtx = new UCTNode(it->second, it->first, expand_treshold);
            if (it->second != FastBoard::PASS) {
                // atari giving
                // was == 2, == 1
                if (board.minimum_elib_count(board.get_to_move(), it->second) <= 2) {
                    vtx->set_expand_cnt(expand_treshold / 3);
                }
                if (board.minimum_elib_count(!board.get_to_move(), it->second) == 1) {
                    vtx->set_expand_cnt(expand_treshold / 3);
                }
            }
            link_child(vtx);
            childrenadded++;
        }
        childrenseen++;
    }

    nodecount += childrenadded;
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

void UCTNode::set_expand_cnt(int runs) {
    m_expand_cnt = runs;
}

void UCTNode::update(Playout & gameresult, int color) {
    SMP::Lock lock(get_mutex());
    m_visits++;
    m_ravevisits++;

    // prefer winning with more territory
    float score = gameresult.get_score();

    m_blackwins += 0.05 * score;

    if (score > 0.0f) {
        m_blackwins += 1.0;
    } else if (score == 0.0f) {
        m_blackwins += 0.5;
    }

    if (color == FastBoard::WHITE) {
        if (score < 0.0f) {
            m_ravestmwins += 1.0 + 0.05 * -score;
        }
    } else if (color == FastBoard::BLACK) {
        if (score > 0.0f) {
            m_ravestmwins += 1.0 + 0.05 * score;
        }
    }
}

bool UCTNode::has_children() const {    
    return m_firstchild != NULL;
}

double UCTNode::get_blackwins() const {
    return m_blackwins;
}

void UCTNode::set_visits(int visits) {
    SMP::Lock lock(get_mutex());
    m_visits = visits;
}

void UCTNode::set_blackwins(double wins) {
    SMP::Lock lock(get_mutex());
    m_blackwins = wins;
}

float UCTNode::get_score() const {
    return m_score;
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

int UCTNode::do_extend() const {
    return m_expand_cnt;
}

UCTNode* UCTNode::uct_select_child(int color, bool use_nets) {
    UCTNode * best = NULL;
    float best_value = -1000.0f;
    int childbound;
    int parentvisits = 1; // XXX: this can be 0 now that we sqrt
    float best_probability = 0.0f;
    if (use_nets) {
        childbound = 35;
    } else {
        childbound = std::max(2, (int)(((log((double)get_visits()) - 3.0) * 3.0) + 2.0));
    }
    SMP::Lock lock(get_mutex());

    int childcount = 0;
    UCTNode * child = m_firstchild;

    // count parentvisits
    float numerator;
    float cutoff_ratio;
    if (use_nets) {
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
        numerator = std::sqrt((float)parentvisits);
        cutoff_ratio = std::max(2.0f, cfg_cutoff_offset +
                                      (cfg_cutoff_ratio * std::log((float)parentvisits)));
    }

    childcount = 0;
    child = m_firstchild;
    // make sure we are at a valid successor
    while (child != NULL && !child->valid()) {
        child = child->m_nextsibling;
    }
    if (use_nets) {
        // first move
        if (child != NULL) {
            best_probability = child->get_score();
        }
    }
    while (child != NULL && childcount < childbound) {
        float value;

        if (use_nets) {
            if (child->get_score() * cutoff_ratio < best_probability) {
                break;
            }

            float c_puct = cfg_puct;
            float c_perbias = cfg_perbias;

            if (!child->first_visit()) {
                // "UCT" part
                float winrate = child->get_winrate(color);
                float psa = child->get_score();
                float denom = 1.0f + child->get_visits();

                value = winrate + c_puct * psa * (numerator / denom) + c_perbias * psa;
            } else {
                float winrate = 1.0f;
                float psa = child->get_score();
                float denom = 1.0f;

                value = winrate + c_puct * psa * (numerator / denom) + c_perbias + psa;
            }
        } else {
            float uctvalue;
            float patternbonus;
            assert(child->get_ravevisits() > 0);
            if (!child->first_visit()) {
                // "UCT" part
                float winrate = child->get_winrate(color);
                uctvalue = winrate;
                patternbonus = sqrtf((child->get_score() * 0.005f) / child->get_visits());
            } else {
                uctvalue = 1.1f;
                patternbonus = sqrtf(child->get_score() * 0.005f);
            }

            // RAVE part
            float ravewinrate = child->get_raverate();
            float ravevalue = ravewinrate + patternbonus;
            float beta = std::max(0.0, 1.0 - log(1.0 + child->get_visits()) / 11.0);

            value = beta * ravevalue + (1.0f - beta) * uctvalue;
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
        if (!a.get<1>() && b.get<1>()) {
            return false;
        }  
        
        if (!b.get<1>() && a.get<1>()) {
            return true;
        }
        
        if (!a.get<1>() && !b.get<1>()) {
            if ((a.get<2>())->get_score() > (b.get<2>())->get_score()) {
                return true;
            } else {
                return false;
            }
        }            
        
        // first check: are playouts comparable and sufficient?
        // then winrate counts

        if (a.get<1>() > UCTSearch::MCTS_MATURE_TRESHOLD
            && b.get<1>() > UCTSearch::MCTS_MATURE_TRESHOLD
            && a.get<1>() * 2 > m_maxvisits
            && b.get<1>() * 2 > m_maxvisits) {

            if (a.get<0>() == b.get<0>()) {
                if (a.get<1> ()> b.get<1>()) {
                    return true;
                } else {
                    return false;
                }
            } else if (a.get<0>() > b.get<0>()) {
                return true;
            } else {
                return false;
            } 
        } else {        
            // playout amount differs greatly, prefer playouts                       
            if (a.get<1>() > b.get<1>()) {
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
*/        
void UCTNode::sort_children(int color) {
    SMP::Lock lock(get_mutex());             
    std::vector<sortnode_t> tmp;
    
    UCTNode * child = m_firstchild;    
    int maxvisits = 0;
    
    while (child != NULL) {        
        int visits = child->get_visits();
		if (visits) {
			tmp.push_back(boost::make_tuple(child->get_winrate(color), visits, child));
		} else {
			tmp.push_back(boost::make_tuple(0.0f, 0, child));
		}
        
        maxvisits = std::max(maxvisits, visits);                
                        
        child = child->m_nextsibling;       
    }        
    
    // reverse sort, because list reconstruction is backwards
    // XXX can be combined?
    std::stable_sort(tmp.begin(), tmp.end(), NodeComp(maxvisits));        
    std::reverse(tmp.begin(), tmp.end());
    
    m_firstchild = NULL;
    
    std::vector<sortnode_t>::iterator it;
    
    for (it = tmp.begin(); it != tmp.end(); ++it) {
        link_child(it->get<2>());   
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
                SMP::Lock lock(child->get_mutex());    
                child->m_ravevisits++;

                if (score > 0.0f) {
                    child->m_ravestmwins += 1.0f + 0.05f * score;
                } else if (score == 0.0f) {
                    child->m_ravestmwins += 0.5f;
                }
            }
        } else {
            bool wpass = playout.passthrough(FastBoard::WHITE, move);        
            
            if (wpass) { 
                SMP::Lock lock(child->get_mutex());    
                child->m_ravevisits++;

                if (score < 0.0f) {
                    child->m_ravestmwins += 1.0f + 0.05f * -score;
                } else if (score == 0.0f) {
                    child->m_ravestmwins += 0.5f;
                }
            }
        }
                        
        child = child->m_nextsibling;       
    }      
}
