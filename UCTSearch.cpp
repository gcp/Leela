#include "config.h"

#include <assert.h>
#include <math.h>
#include <vector>
#include <utility>
#include <boost/thread.hpp>

#include "FastBoard.h"
#include "UCTSearch.h"
#include "Timing.h"
#include "Random.h"
#include "Utils.h"
#include "TTable.h"
#include "MCOTable.h"

using namespace Utils;

UCTSearch::UCTSearch(GameState & g)
: m_rootstate(g), 
  m_nodes(0), 
  m_score(0.0f), 
  m_root(g.get_to_move(), FastBoard::PASS, 0.0f),
  m_maxvisits(UCTSearch::MAX_TREE_SIZE * 100), 
  m_hasrunflag(false), 
  m_runflag(NULL),
  m_analyzing(false) {    
}

void UCTSearch::set_runflag(bool * flag) {
    m_runflag = flag;
    m_hasrunflag = true;
}

Playout UCTSearch::play_simulation(KoState & currstate, UCTNode* const node) {
    const int color = currstate.get_to_move();
    const uint64 hash = currstate.board.get_hash();
    Playout noderesult;  
        
    TTable::get_TT()->sync(hash, node);        

    bool has_children = node->has_children();

    if (has_children == false && node->get_visits() <= MATURE_TRESHOLD) {           
        noderesult.run(currstate);                
    } else {                
        if (has_children == false && m_nodes < MAX_TREE_SIZE) {                
            m_nodes += node->create_children(currstate);
        }        
                
        if (node->has_children() == true) {                        
            UCTNode * next = node->uct_select_child(color); 
            
            if (next != NULL) {
                int move = next->get_move();            
                
                if (move != FastBoard::PASS) {                
                    currstate.play_move(move);
                    
                    if (!currstate.superko()) {                    
                        noderesult = play_simulation(currstate, next);                                        
                    } else {                                            
                        next->invalidate();                       
                        noderesult.run(currstate);                         
                    }                
                } else {                
                    currstate.play_pass();                
                    noderesult = play_simulation(currstate, next);                
                }       
            } else {
                noderesult.run(currstate);
            }
            
            node->updateRAVE(noderesult, color);                        
        } else if (m_nodes >= MAX_TREE_SIZE) {
            noderesult.run(currstate);
        } else {                     
            noderesult.set_final_score(currstate.percentual_area_score()); 
        }        
    }             
      
    node->update(noderesult, !color);    
    TTable::get_TT()->update(hash, node);    
    
    return noderesult;  
}

void UCTSearch::dump_stats(GameState & state, UCTNode & parent) {
    const int color = state.get_to_move();
    int bestmove = FastBoard::PASS;
    int bestvisits = 0;
    float bestrate = 0.0f;          
    
    if (!parent.has_children()) {
        return;
    }

    // sort children, put best move on top    
    m_root.sort_children(color);

    UCTNode * bestnode = parent.get_first_child();   
    
    if (bestnode->first_visit()) {
        return;
    }    
    
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
                  node->get_visits() > 0 ? node->get_raverate()*100.0f : 0.0f,
                  node->get_ravevisits(),
                  tmp.c_str());
        
        
        GameState tmpstate = state;  
        
        tmpstate.play_move(node->get_move());                
        myprintf(get_pv(tmpstate, *node).c_str()); 
                              
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
    myprintf(get_pv(tmpstate, parent).c_str()); 
                          
    myprintf("\n");
}

void UCTSearch::dump_thinking() {
    GameState tempstate = m_rootstate;   
    int color = tempstate.board.m_tomove;
    myprintf("Nodes: %d, Winrate: %5.2f%%, PV: ", 
              m_root.get_visits(), m_root.get_winrate(color) * 100.0f);
    myprintf(get_pv(tempstate, m_root).c_str());
    myprintf("\n");
    
    std::string bestmove;
    if (m_root.get_first_child()) {
        bestmove = m_rootstate.move_to_text(m_root.get_first_child()->get_move());        
    } else {
        bestmove = "pass";
    }        
    GUIprintf("Nodes: %d, Best move: %s", 
              m_root.get_visits(), bestmove.c_str());
}

bool UCTSearch::allow_early_exit() {    
    int color = m_rootstate.board.m_tomove;    
    
    if (!m_root.has_children()) {
        return false;
    }
    
    m_root.sort_children(color);
    
    // do we have statistics on the moves?
    UCTNode * first = m_root.get_first_child();
    if (first != NULL) {
        if (first->first_visit()) {
            return false;
        }
    } else {
        return false;
    }
    
    UCTNode * second = first->get_sibling();    
    if (second != NULL) {
        if (second->first_visit()) {
            return false;
        }
    } else {
        return false;
    }
    
    float n1 = first->get_visits();
    float p1 = first->get_winrate(color);
    float n2 = second->get_visits();
    float p2 = second->get_winrate(color);
    
    float low, high;
        
    low  = p1 - 3.0f * sqrtf(0.25f / n1);
    high = p2 + 3.0f * sqrtf(0.25f / n2);    
    
    if (low > high) {
        myprintf("Allowing early exit: low: %f > high: %f\n", low, high);
        return true;
    }
    
    return false;    
}

int UCTSearch::get_best_move(passflag_t passflag) { 
    int color = m_rootstate.board.m_tomove;    

    // make sure best is first
    m_root.sort_children(color);
    
    int bestmove = m_root.get_first_child()->get_move();    
    
    // do we have statistics on the moves?
    if (m_root.get_first_child() != NULL) {
        if (m_root.get_first_child()->first_visit()) {
            return bestmove;
        }
    }
    
    float bestscore = m_root.get_first_child()->get_winrate(color);   
    int visits = m_root.get_first_child()->get_visits();
    
    m_score = bestscore;        

    // do we want to fiddle with the best move because of the rule set?
     if (passflag & UCTSearch::NOPASS) {
        // were we going to pass?
        if (bestmove == FastBoard::PASS) {
            UCTNode * nopass = m_root.get_nopass_child();
            
            if (nopass != NULL) {
                myprintf("Preferring not to pass.\n");
                bestmove = nopass->get_move();
                if (nopass->first_visit()) {
                    bestscore = 1.0f;
                } else {
                    bestscore = nopass->get_winrate(color);
                }
            } else {
                myprintf("Pass is the only acceptable move.\n");
            }
        }
    } else {
        if (m_rootstate.get_last_move() == FastBoard::PASS) {
            if (m_root.get_pass_child() != NULL) {
                if (m_root.get_pass_child()->get_visits() > 100) {
                    float passscore = m_root.get_pass_child()->get_winrate(color);
                    
                    // is passing a winning move?
                    if (passscore > 0.90f) {                    
                        // is passing within 5% of the best move?                                   
                        if (bestscore - passscore < 0.05f) {
                            myprintf("Preferring to pass since it's %5.2f%% compared to %5.2f%%.\n", 
                                      passscore * 100.0f, bestscore * 100.0f);
                            bestmove = FastBoard::PASS;                
                        }
                    }
                }            
            }
        }
        // either by forcing or coincidence passing is
        // on top...check whether passing loses instantly        
        if (bestmove == FastBoard::PASS) {
            // do full count including dead stones
            float score = m_rootstate.final_score();
            // do we lose by passing?
            if ((score > 0.0f && color == FastBoard::WHITE)
                ||
                (score < 0.0f && color == FastBoard::BLACK)) {
                myprintf("Passing loses :-(\n");
                // find a valid non-pass move
                UCTNode * nopass = m_root.get_nopass_child();
            
                if (nopass != NULL) {
                    myprintf("Avoiding pass because it loses.\n");
                    bestmove = nopass->get_move();
                    if (nopass->first_visit()) {
                        bestscore = 1.0f;
                    } else {
                        bestscore = nopass->get_winrate(color);
                    }
                } else {
                    myprintf("No alternative to passing.\n");
                }
            } else {
                myprintf("Passing wins :-)\n");
            }
        }    
    } 
    
    // if we aren't passing, should we consider resigning?
    if (bestmove != FastBoard::PASS) {
        // resigning allowed
        if ((passflag & UCTSearch::NORESIGN) == 0) {
            int movetresh = (m_rootstate.board.get_boardsize() 
                             * m_rootstate.board.get_boardsize()) / 3;
            // bad score and visited enough
            if (bestscore < 0.05f 
                && visits > 90 
                && m_rootstate.movenum > movetresh) {
                myprintf("Score looks bad. Resigning.\n");
                bestmove = FastBoard::RESIGN;    
            }
        }
    }
       
    return bestmove;
}

void UCTSearch::dump_order2(void) {            
    std::vector<int> moves = m_rootstate.generate_moves(m_rootstate.get_to_move());
    std::vector<std::pair<float, std::string> > ord_list;            
        
    std::vector<int> territory = m_rootstate.board.influence();
    std::vector<int> moyo = m_rootstate.board.moyo();
    for (int i = 0; i < moves.size(); i++) {
        if (moves[i] > 0) {
            ord_list.push_back(std::make_pair(
                               //AMAFTable::get_AMAFT()->get_score(m_rootstate.get_to_move(), moves[i]),
                               m_rootstate.score_move(territory, moyo, moves[i]), 
                               m_rootstate.move_to_text(moves[i])));
        }
    } 
    
    std::sort(ord_list.rbegin(), ord_list.rend());
    
    myprintf("\nOrder Table\n");
    myprintf("--------------------\n");
    for (unsigned int i = 0; i < std::min<int>(10, ord_list.size()); i++) {
        myprintf("%4s -> %10.10f\n", ord_list[i].second.c_str(), 
                                     ord_list[i].first); 
    }
    myprintf("--------------------\n");        
}

std::string UCTSearch::get_pv(GameState & state, UCTNode & parent) {
    if (!parent.has_children()) {
        return std::string();
    }
    
    parent.sort_children(state.get_to_move());                    
        
    UCTNode * bestchild = parent.get_first_child();    
    
    if (bestchild->get_visits() <= MATURE_TRESHOLD) {
        return std::string();
    }
    
    int bestmove = bestchild->get_move();
                   
    std::string tmp = state.move_to_text(bestmove);    
    
    std::string res(tmp);
    res.append(" ");    
    
    state.play_move(bestmove);    
    
    std::string next = get_pv(state, *bestchild);
    
    res.append(next);
    
    return res;
}

void UCTSearch::dump_analysis(void) {
    GameState tempstate = m_rootstate;   
    int color = tempstate.board.m_tomove;
                
    std::string pvstring = get_pv(tempstate, m_root);
    
    float winrate = m_root.get_winrate(color) * 100.0f;
    winrate = std::max(0.0f, winrate);
    winrate = std::min(100.0f, winrate);
        
    GUIprintf("Nodes: %d, Win: %5.2f%%, PV: %s", m_root.get_visits(), 
               winrate, pvstring.c_str());   
}

bool UCTSearch::is_running() {
    return m_run;
}

void UCTWorker::operator()() {    
    do {
        KoState currstate = m_rootstate;
        m_search->play_simulation(currstate, m_root); 
    } while(m_search->is_running()); 
}

float UCTSearch::get_score() {
    return m_score;
}

int UCTSearch::think(int color, passflag_t passflag) {
    // set side to move
    m_rootstate.board.m_tomove = color;

    // set up timing info
    Time start;
    int centiseconds_elapsed;
    int time_for_move;
    int last_update = 0;
    
    if (!m_analyzing) {
        time_for_move = m_rootstate.get_timecontrol()->max_time_for_move(color);       
    
        GUIprintf("Thinking at most %.2f seconds", time_for_move/100.0f);
    } else {
        time_for_move = MAX_TREE_SIZE * 100;
                
        GUIprintf("Analyzing...");
    }
    
    //XXX: testing
    //m_maxvisits = 10000;
    int max_iterations = 5000;    
                 
    m_rootstate.start_clock(color);

    // do some preprocessing for move ordering
    MCOwnerTable::clear();  
    float score = Playout::mc_owner(m_rootstate, 64);                   
        
    // create a sorted list off legal moves (make sure we
    // play something legal and decent even in time trouble)
    m_nodes += m_root.create_children(m_rootstate, true);
    m_root.kill_superkos(m_rootstate);        
    
    m_run = true;            
    int cpus = SMP::get_num_cpus();        
    //int cpus = 2;
    boost::thread_group tg;         
#ifdef USE_SMP       
    for (int i = 1; i < cpus; i++) {         
        tg.create_thread(UCTWorker(m_rootstate, this, &m_root));
    }        
#endif    
    bool keeprunning = true;   
    int iterations = 0; 
    do {
        KoState currstate = m_rootstate;
                
        play_simulation(currstate, &m_root);                   

        Time elapsed;
        centiseconds_elapsed = Time::timediff(start, elapsed);        

        // output some stats every second
        // check if we should still search
        if (!m_analyzing) {
            if (centiseconds_elapsed - last_update > 250) {
                last_update = centiseconds_elapsed;            
                dump_thinking();                        
            }  
            keeprunning = (centiseconds_elapsed < time_for_move 
                           && m_root.get_visits() < m_maxvisits
                           && (!m_hasrunflag || (*m_runflag)));
                           
            // check for early exit                           
            if (keeprunning && ((iterations & 127) == 0) && centiseconds_elapsed > time_for_move/2) {
                keeprunning = !allow_early_exit();    
            }                           
        } else {
            if (centiseconds_elapsed - last_update > 100) {
                last_update = centiseconds_elapsed;            
                dump_analysis();                        
            }  
            keeprunning = (!m_hasrunflag || (*m_runflag));
        }   
        if (iterations++ > max_iterations) {
            keeprunning = false;
        }     
    } while(keeprunning);
    
    // stop the search
    m_run = false;
    tg.join_all();                                        
    
    if (!m_root.has_children()) {
        return FastBoard::PASS;
    }
    
    m_rootstate.stop_clock(color);     
        
    // display search info        
    myprintf("\n");
    dump_stats(m_rootstate, m_root);                  
        
    if (centiseconds_elapsed > 0) {    
        myprintf("\n%d visits, %d nodes, %d vps\n\n", 
                 m_root.get_visits(), 
                 m_nodes,
                 (m_root.get_visits() * 100) / (centiseconds_elapsed+1));  
        GUIprintf("%d visits, %d nodes, %d vps", 
                 m_root.get_visits(), 
                 m_nodes,
                 (m_root.get_visits() * 100) / (centiseconds_elapsed+1));                                                     
    }             
                
    int bestmove = get_best_move(passflag);
    
    GUIprintf("Best move: %s", m_rootstate.move_to_text(bestmove).c_str());
       
    return bestmove;
}

void UCTSearch::ponder() {                          
    MCOwnerTable::clear();  
    Playout::mc_owner(m_rootstate, 64);             
         
    m_run = true;
    //int cpus = SMP::get_num_cpus();       
    int cpus = 2;     
    boost::thread_group tg;        
#ifdef USE_SMP        
    for (int i = 1; i < cpus; i++) {         
        tg.create_thread(UCTWorker(m_rootstate, this, &m_root));
    }        
#endif    
    do {
        KoState currstate = m_rootstate;                
        play_simulation(currstate, &m_root);                             
    } while(!Utils::input_pending() && (!m_hasrunflag || (*m_runflag)));  
           
    // stop the search
    m_run = false;    
    tg.join_all();                                            
                        
    // display search info        
    myprintf("\n");
    dump_stats(m_rootstate, m_root);                  
               
    myprintf("\n%d visits, %d nodes\n\n", m_root.get_visits(), m_nodes);                                 
}

void UCTSearch::set_visit_limit(int visits) {
    if (visits == 0) {
        m_maxvisits = UCTSearch::MAX_TREE_SIZE * 100;
    } else {
        m_maxvisits = visits;
    }
}

void UCTSearch::set_analyzing(bool flag) {
    m_analyzing = flag;
}