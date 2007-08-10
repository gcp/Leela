#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
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
    
    std::vector<UCTNode>::iterator child = node->first_child();
    
    for (;child != node->end_child(); ++child) {    
        float uctvalue;
        
        if (!child->first_visit()) {
            float winrate = child->get_winrate(m_currstate.get_to_move());            
            //float uct = 1.0f * sqrtf(logf(node->get_visits())/(5.0f * child->get_visits()));
            
            float logparent = logf(node->get_visits());
            float childfactor = logparent / child->get_visits();
            
            float uct_v = winrate - (winrate * winrate) + sqrtf(2.0f * childfactor);
            float uncertain = sqrt(childfactor * min(0.25f, uct_v));
            float uct = uncertain;
            
            uctvalue = winrate + 1.2f * uct;
        } else {            
            //uctvalue = 10000 + Random::get_Rng()->random();
            uctvalue = 1.0f;
        }

        if (uctvalue >= best_uct) {
            best_uct = uctvalue;
            result = &(*child);
        }            
    }
    
    return result;
}

float UCTSearch::play_simulation(UCTNode* node) {
    float noderesult;

    if (node->get_visits() < 100) {
        noderesult = node->do_one_playout(m_currstate);
    } else {
        if (node->has_children() == false) {
            node->create_children(m_currstate);
        }

        UCTNode* next = uct_select(node); 

        int move = next->get_move();
        
        if (move != FastBoard::PASS) {
            m_currstate.play_move_fast(move);
        } else {
            m_currstate.play_pass_fast();
        }            
        
        noderesult = play_simulation(next);
    }

    node->add_visit();
    
    node->update(noderesult);  
    
    return noderesult;  
}

static void dump_pv(GameState & state, UCTNode & parent) {
    int bestmove;
    int bestvisits = 0;
    UCTNode bestchild;
    
    BOOST_FOREACH(UCTNode & node, std::make_pair(parent.first_child(), parent.end_child())) {                              
        if (node.get_visits() >= bestvisits) {
            bestmove = node.get_move();
            bestchild = node;
            bestvisits = node.get_visits();
        }                  
    }        
    
    char tmp[16];                
    state.move_to_text(bestchild.get_move(), tmp);
    
    myprintf("%s ", tmp);
    
    if (!bestchild.has_children()) {
        return;
    }
    
    state.play_move(bestmove);    
    
    dump_pv(state, bestchild);
}

int UCTSearch::think(int color) {
    Time start;
    int time_for_move = 500;

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
    
    int bestmove = FastBoard::PASS;
    int bestvisits = 0;
    float bestrate = 0.0f;    
    
    BOOST_FOREACH(UCTNode & node, std::make_pair(m_root.first_child(), m_root.end_child())) {
        char tmp[16];
        
        m_rootstate.move_to_text(node.get_move(), tmp);
        
        myprintf("%4s -> %7d (%5.2f%%)\n", 
                  tmp, 
                  node.get_visits(), 
                  node.get_winrate(m_rootstate.get_to_move())*100.0f);
                  
        if (node.get_visits() >= bestvisits) {
            bestmove = node.get_move();
            bestvisits = node.get_visits();
            bestrate = node.get_winrate(m_rootstate.get_to_move());
        }                  
    }
    
    GameState tmpstate = m_rootstate;
    
    myprintf("PV: ");
    dump_pv(tmpstate, m_root);    
    
    myprintf("(%5.2f%%) -> %5.2f%%\n", 
             m_root.get_winrate(m_rootstate.get_to_move()) * 100.0f, 
             bestrate * 100.0f);  
    myprintf("\n%d nodes, %d nps\n\n", m_nodes, (m_nodes * 100) / centiseconds_elapsed);             

    return bestmove;
}