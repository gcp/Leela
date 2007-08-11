#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <functional>
#include <boost/foreach.hpp>

#include "config.h"

#include "FastBoard.h"
#include "UCTSearch.h"
#include "Timing.h"
#include "Random.h"
#include "Utils.h"

using namespace Utils;

UCTSearch::UCTSearch(GameState &g)
: m_rootstate(g) {       
}

UCTNode* UCTSearch::uct_select(UCTNode* node) {
    float best_uct = 0.0f;
    UCTNode* result = NULL;
    
    UCTNode::iterator child = node->begin();
    
    for (;child != node->end(); ++child) {    
        float uctvalue;
        
        if (child->get_visits() >= MATURE_TRESHOLD) {
            float winrate = child->get_winrate(m_currstate.get_to_move());            
            //float uct = 1.0f * sqrtf(logf(node->get_visits())/(5.0f * child->get_visits()));
            
            float logparent = logf(node->get_visits());
            float childfactor = logparent / child->get_visits();
            
            float uct_v = winrate - (winrate * winrate) + sqrtf(2.0f * childfactor);
            float uncertain = sqrt(childfactor * min(0.25f, uct_v));
            float uct = uncertain;
            
            uctvalue = winrate + 1.2f * uct;
            //uctvalue = winrate + uct;
        } else {            
            //uctvalue = 10000 + Random::get_Rng()->random();
            uctvalue = 1.0f;
        }

        if (uctvalue >= best_uct) {
            best_uct = uctvalue;
            result = &(*child);
        }            
    }
    
    assert(result != NULL);
    
    return result;
}

float UCTSearch::play_simulation(UCTNode* node) {
    float noderesult;

    if (node->get_visits() < MATURE_TRESHOLD) {
        noderesult = node->do_one_playout(m_currstate);
    } else {
        if (node->has_children() == false) {
            node->create_children(m_currstate);                                             
        }
                
        if (node->has_children() == true) {
            UCTNode* next = uct_select(node); 

            int move = next->get_move();
            
            if (move != FastBoard::PASS) {
                m_currstate.play_move_fast(move);
            } else {
                m_currstate.play_pass_fast();
            }            
            
            noderesult = play_simulation(next);
        } else {
            // terminal node, handle this smarter
            noderesult = node->do_one_playout(m_currstate);
        }        
    }

    node->add_visit();    
    node->update(noderesult);  
    
    return noderesult;  
}

class NodeComp : public std::binary_function<UCTNode&, UCTNode&, bool> {   
private:
    const int m_color;
public:   
    NodeComp(const int color) : m_color(color) {}
   
    bool operator()(const UCTNode & a, const UCTNode & b) {  
        if (a.first_visit() && !b.first_visit()) {
            return false;
        }  
        if (b.first_visit() && !a.first_visit()) {
            return true;
        }
        if (a.first_visit() && b.first_visit()) {
            return false;
        }
        if (a.get_visits() == b.get_visits()) {
            if (a.get_winrate(m_color) > b.get_winrate(m_color)) {
                return true;
            } else {
                return false;
            }
        } else if (a.get_visits() > b.get_visits()) {
            return true;
        } else {
            return false;
        }        
    }
};

void UCTSearch::dump_pv(GameState & state, UCTNode & parent) {
    
    if (!parent.has_children()) {
        return;
    }
    
    std::stable_sort(parent.begin(), parent.end(), NodeComp(state.get_to_move()));
        
    UCTNode* bestchild = &(*parent.begin());
    int bestmove = bestchild->get_move();
    
    char tmp[16];                
    state.move_to_text(bestmove, tmp);
    
    myprintf("%s ", tmp);             
    
    state.play_move(bestmove);    
    
    dump_pv(state, *bestchild);
}

void UCTSearch::dump_stats(GameState & state, UCTNode & parent) {
    const int color = state.get_to_move();
    int bestmove = FastBoard::PASS;
    int bestvisits = 0;
    float bestrate = 0.0f;      
    UCTNode* bestnode = NULL;
    
    bestnode = &(*parent.begin());
    
    bestrate = bestnode->get_winrate(color);
    bestvisits = bestnode->get_visits();
    bestmove = bestnode->get_move();
        
    int movecount = 0;
    
    BOOST_FOREACH(UCTNode & node, parent) {
    
        if (++movecount > 6) break;
    
        char tmp[16];
        
        state.move_to_text(node.get_move(), tmp);
        
        myprintf("%4s -> %7d (%5.2f%%) PV: %s ", 
                  tmp, 
                  node.get_visits(), 
                  node.get_visits() > 0 ? node.get_winrate(color)*100.0f : 0.0f,
                  tmp);
        
        
        GameState tmpstate = state;  
        
        tmpstate.play_move(node.get_move());                
        dump_pv(tmpstate, node);                                        
                              
        myprintf("\n");                                                                          
    }     
    
    char tmp[16];
    state.move_to_text(bestnode->get_move(), tmp);
    myprintf("====================================\n"
             "%d visits, score %5.2f%% (from %5.2f%%) PV: ",   
             bestnode->get_visits(), 
             bestnode->get_visits() > 0 ? bestnode->get_winrate(color)*100.0f : 0.0f,
             parent.get_winrate(color) * 100.0f,             
             tmp);    
                      
    GameState tmpstate = state;                                
    dump_pv(tmpstate, parent);      
                          
    myprintf("\n");
}

int UCTSearch::think(int color) {
    Time start;
    int time_for_move = 200;

    m_nodes = 0;    
    m_rootstate.board.m_tomove = color;
    
    int centiseconds_elapsed;
    
    do {
        m_currstate = m_rootstate;
        play_simulation(&m_root);

        m_nodes++;        

        Time elapsed;
        centiseconds_elapsed = Time::timediff(start, elapsed);
    } while (centiseconds_elapsed < time_for_move);  
    
    // sort children, put best move on top    
    std::stable_sort(m_root.begin(), m_root.end(), NodeComp(color));
            
    dump_stats(m_rootstate, m_root);                          
        
    myprintf("\n%d nodes, %d nps\n\n", m_nodes, (m_nodes * 100) / centiseconds_elapsed);             
    
    int bestmove = m_root.begin()->get_move();

    return bestmove;
}