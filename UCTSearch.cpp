#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

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

float UCTSearch::play_simulation(UCTNode* node) {
    float noderesult;

    if (node->get_visits() < MATURE_TRESHOLD) {
        noderesult = node->do_one_playout(m_currstate);
    } else {
        if (node->has_children() == false) {
            node->create_children(m_currstate);                                             
        }
                
        if (node->has_children() == true) {
            UCTNode* next = node->uct_select_child(m_currstate.get_to_move()); 

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

void UCTSearch::dump_pv(GameState & state, UCTNode & parent) {
    
    if (!parent.has_children()) {
        return;
    }
    
    parent.sort_children(state.get_to_move());                    
        
    UCTNode * bestchild = parent.get_first_child();
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
    
    UCTNode* bestnode = parent.get_first_child();
    
    bestrate = bestnode->get_winrate(color);
    bestvisits = bestnode->get_visits();
    bestmove = bestnode->get_move();
                    
    int movecount = 0;
    UCTNode * node = bestnode;
    
    while (node != NULL) {            
        if (++movecount > 6) break;
    
        char tmp[16];
        
        state.move_to_text(node->get_move(), tmp);
        
        myprintf("%4s -> %7d (%5.2f%%) PV: %s ", 
                  tmp, 
                  node->get_visits(), 
                  node->get_visits() > 0 ? node->get_winrate(color)*100.0f : 0.0f,
                  tmp);
        
        
        GameState tmpstate = state;  
        
        tmpstate.play_move(node->get_move());                
        dump_pv(tmpstate, *node);                                        
                              
        myprintf("\n");   
        
        node = node->get_sibling();                                                                       
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
    int time_for_move = 1000;

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
    m_root.sort_children(color);
            
    dump_stats(m_rootstate, m_root);                          
        
    myprintf("\n%d nodes, %d nps\n\n", m_nodes, (m_nodes * 100) / centiseconds_elapsed);             
    
    int bestmove = m_root.get_first_child()->get_move();

    return bestmove;
}