#include <assert.h>
#include <math.h>
#include <vector>
#include <utility>

#include "config.h"

#include "FastBoard.h"
#include "UCTSearch.h"
#include "Timing.h"
#include "Random.h"
#include "Utils.h"
#include "TTable.h"
#include "HistoryTable.h"

using namespace Utils;

UCTSearch::UCTSearch(GameState & g)
: m_rootstate(g), m_nodes(0) {    
}

Playout UCTSearch::play_simulation(UCTNode* node) {
    const int color = m_currstate.get_to_move();
    const uint64 hash = m_currstate.board.get_hash();
    Playout noderesult;  
    
    //TTable::get_TT()->sync(hash, node);      

    if (node->get_visits() <= MATURE_TRESHOLD) {
        noderesult.run(m_currstate);
    } else {
        if (node->has_children() == false) {
            m_nodes += node->create_children(m_currstate);                                             
        }
                
        if (node->has_children() == true) {                        
            UCTNode* next = node->uct_select_child(color); 

            int move = next->get_move();            
            
            if (move != FastBoard::PASS) {
                m_currstate.play_move(move);
                
                if (!m_currstate.superko()) {
                    noderesult = play_simulation(next);                    
                } else {
                    node->delete_child(next);   
                    noderesult.run(m_currstate);     
                }
            } else {
                m_currstate.play_pass();
                
                noderesult = play_simulation(next);
            }       
                    
            node->updateRAVE(noderesult, color);                        
        } else {                     
            noderesult.set_final_score(m_currstate.board.percentual_area_score(m_currstate.m_komi));            
            node->finalize(noderesult.get_score());
        }        
    }             
    
    node->update(noderesult, !color);    
    //TTable::get_TT()->update(hash, node);    
    
    return noderesult;  
}

void UCTSearch::dump_pv(GameState & state, UCTNode & parent) {
    
    if (!parent.has_children()) {
        return;
    }
    
    parent.sort_children(state.get_to_move());                    
        
    UCTNode * bestchild = parent.get_first_child();    
    
    if (bestchild->get_visits() <= MATURE_TRESHOLD) {
        return;
    }
    
    int bestmove = bestchild->get_move();
                   
    std::string tmp = state.move_to_text(bestmove);    
    myprintf("%s ", tmp.c_str());
    
    state.play_move(bestmove);    
    
    dump_pv(state, *bestchild);
}

void UCTSearch::dump_stats(GameState & state, UCTNode & parent) {
    const int color = state.get_to_move();
    int bestmove = FastBoard::PASS;
    int bestvisits = 0;
    float bestrate = 0.0f;          
    
    // sort children, put best move on top    
    m_root.sort_children(color);

    UCTNode* bestnode = parent.get_first_child();       
    
    bestrate = bestnode->get_winrate(color);
    bestvisits = bestnode->get_visits();
    bestmove = bestnode->get_move();
                    
    int movecount = 0;
    UCTNode * node = bestnode;
    
    while (node != NULL) {            
        if (++movecount > 6) break;            
        
        std::string tmp = state.move_to_text(node->get_move());
        
        myprintf("%4s -> %7d (U: %5.2f%%) (R: %5.2f%%: %7d) PV: %s ", 
                  tmp.c_str(), 
                  node->get_visits(), 
                  node->get_visits() > 0 ? node->get_winrate(color)*100.0f : 0.0f,
                  node->get_visits() > 0 ? node->get_raverate(color)*100.0f : 0.0f,
                  node->get_ravevisits(),
                  tmp.c_str());
        
        
        GameState tmpstate = state;  
        
        tmpstate.play_move(node->get_move());                
        dump_pv(tmpstate, *node);                                        
                              
        myprintf("\n");   
        
        node = node->get_sibling();                                                                       
    }     
        
    std::string tmp = state.move_to_text(bestnode->get_move());
    myprintf("====================================\n"
             "%d visits, score %5.2f%% (from %5.2f%%) PV: ",   
             bestnode->get_visits(), 
             bestnode->get_visits() > 0 ? bestnode->get_winrate(color)*100.0f : 0.0f,
             parent.get_winrate(color) * 100.0f,             
             tmp.c_str());    
                      
    GameState tmpstate = state;                                
    dump_pv(tmpstate, parent);      
                          
    myprintf("\n");
}

void UCTSearch::dump_thinking() {
    GameState tempstate = m_rootstate;   
    int color = tempstate.board.m_tomove;
    myprintf("Nodes: %d, Winrate: %5.2f%%, PV: ", 
              m_root.get_visits(), m_root.get_winrate(color) * 100.0f);
    dump_pv(tempstate, m_root);
    myprintf("\n");
}

int UCTSearch::get_best_move(passflag_t passflag) { 
    int color = m_rootstate.board.m_tomove;    

    // make sure best is first
    m_root.sort_children(color);
    
    float bestscore = m_root.get_first_child()->get_winrate(color);       
    int bestmove = m_root.get_first_child()->get_move();    

    // do we want to fiddle with the best move because of the rule set?
    if (passflag == UCTSearch::PREFERPASS) {
        if (!m_root.get_pass_child()->first_visit()) {
            float passscore = m_root.get_pass_child()->get_winrate(color);
            
            // is passing a winning move?
            if (passscore > 0.85f) {
                
                // is passing within 5% of the best move?            
                if (bestscore - passscore < 0.05f) {
                    myprintf("Preferring to pass since it's %5.2f%% compared to %5.2f%%.\n", 
                              passscore * 100.0f, bestscore * 100.0f);
                    bestmove = FastBoard::PASS;                
                }
            }
        }
    } else if (passflag == UCTSearch::NOPASS) {
        // were we going to pass?
        if (bestmove == FastBoard::PASS) {
            UCTNode * nopass = m_root.get_nopass_child();
            
            if (nopass != NULL) {
                myprintf("Preferring not to pass.\n");
                bestmove = nopass->get_move();
            } else {
                myprintf("Pass is the only acceptable move.\n");
            }
        }
    }

    return bestmove;
}

void UCTSearch::dump_history(void) {        
    HistoryTable * ht = HistoryTable::get_HT();
    
    std::vector<std::pair<std::pair<float, int>, std::string> > ht_list;
    
    for (int i = 0; i < FastBoard::MAXSQ; i++) {                
        if (ht->get_visits(i) > 5) {            
            ht_list.push_back(std::make_pair(std::make_pair(ht->get_score(i) * 100.0f, 
                                                            ht->get_visits(i)),
                              m_rootstate.move_to_text(i)));
        }
    }
    
    std::sort(ht_list.rbegin(), ht_list.rend());
    
    myprintf("\nHistory Table\n");
    myprintf("---------------\n");
    for (unsigned int i = 0; i < min(6, ht_list.size()); i++) {
        myprintf("%4s -> %5.2f%% (%7d)\n", ht_list[i].second.c_str(), 
                                           ht_list[i].first.first, 
                                           ht_list[i].first.second);
    }
    myprintf("---------------\n");        
}

void UCTSearch::dump_order(void) {            
    
    std::vector<std::pair<float, std::string> > ord_list;
    
    UCTNode* node = m_root.get_first_child();
    
    while (node != NULL) {
        if (node->get_move() != FastBoard::PASS) {
            ord_list.push_back(std::make_pair(
                               m_rootstate.score_move(m_rootstate.get_to_move(), node->get_move()), 
                               m_rootstate.move_to_text(node->get_move())));
        } else {
            ord_list.push_back(std::make_pair(0.0f, "pass"));
        }
        node = node->get_sibling();                                                                       
    }    
    
    std::sort(ord_list.rbegin(), ord_list.rend());
    
    myprintf("\nOrder Table\n");
    myprintf("-------------------\n");
    for (unsigned int i = 0; i < min(10, ord_list.size()); i++) {
        myprintf("%4s -> %10.2f\n", ord_list[i].second.c_str(), 
                                    ord_list[i].first);                              
    }
    myprintf("-------------------\n");        
}

int UCTSearch::think(int color, passflag_t passflag) {
    // set side to move
    m_rootstate.board.m_tomove = color;

    // set up timing info
    Time start;
    int centiseconds_elapsed;
    int last_update = 0;

    int time_for_move = m_rootstate.get_timecontrol()->max_time_for_move(color);       
    m_rootstate.start_clock(color);
    
    HistoryTable::get_HT()->clear();

    do {
        m_currstate = m_rootstate;

        play_simulation(&m_root);                   

        Time elapsed;
        centiseconds_elapsed = Time::timediff(start, elapsed);        

        // output some stats every 2.5 seconds
        if (centiseconds_elapsed - last_update > 250) {
            last_update = centiseconds_elapsed;
            dump_thinking();            
        }        
    } while(/*centiseconds_elapsed < time_for_move*/ m_root.get_visits() < 20000); 
    
    if (!m_root.has_children()) {
        return FastBoard::PASS;
    }
    
    m_rootstate.stop_clock(color);     
        
    // display search info        
    myprintf("\n");
    dump_stats(m_rootstate, m_root);
    //dump_history();   
    dump_order();                          
        
    if (centiseconds_elapsed > 0) {    
        myprintf("\n%d visits, %d nodes, %d vps\n\n", 
                 m_root.get_visits(), 
                 m_nodes,
                 (m_root.get_visits() * 100) / (centiseconds_elapsed+1));              
    }             
            
    // XXX: check for pass but no actual win on final_scoring
    int bestmove = get_best_move(passflag);
       
    return bestmove;
}