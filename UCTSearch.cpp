#include "config.h"

#include <assert.h>
#include <math.h>
#include <limits.h>
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
#ifdef USE_NETS
#include "Network.h"
#endif

using namespace Utils;

UCTSearch::UCTSearch(GameState & g)
: m_rootstate(g),
  m_root(FastBoard::PASS, 0.0f),
  m_nodes(0),
  m_maxvisits(INT_MAX),
  m_score(0.0f),
  m_hasrunflag(false),
  m_runflag(NULL),
  m_analyzing(false),
  m_quiet(false) {
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

    if (has_children == false && node->get_visits() <= node->do_extend()) {           
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

    if (!parent.has_children()) {
        return;
    }

    // sort children, put best move on top
    m_root.sort_children(color);

    UCTNode * bestnode = parent.get_first_child();   
    
    if (bestnode->first_visit()) {
        return;
    }    

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

bool UCTSearch::allow_early_exit() {    
    int color = m_rootstate.board.get_to_move();    
    
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
    
    double n1 = first->get_visits();
    double p1 = first->get_winrate(color);
    double n2 = second->get_visits();
    double p2 = second->get_winrate(color);
    double low, high;

    low  = p1 - 3.0 * sqrt(0.25 / n1);
    high = p2 + 3.0 * sqrt(0.25 / n2);
 
    if (low > high) {
        myprintf("Allowing early exit: low: %f%% > high: %f%%\n", low * 100.0, high * 100.0);
        return true;
    }
    if (p1 < 0.10 || p1 > 0.95) {
        myprintf("Allowing early exit: score: %f%%\n", p1 * 100.0);
        return true;
    }

    return false;
}

int UCTSearch::get_best_move_nosearch(std::vector<std::pair<float, int>> moves,
                                      float score, passflag_t passflag) {
    int color = m_rootstate.board.m_tomove;

    // Resort
    std::stable_sort(moves.rbegin(), moves.rend());
    int bestmove = moves[0].second;

    static const size_t min_alternates = 20;
    static const size_t max_consider = 3;

    // Pick proportionally from top 3 moves, if close enough and enought left.
    if (moves.size() > min_alternates) {
        float best_score = moves[0].first;
        float cumul_score = best_score;
        int viable_moves = 1;
        for (size_t i = 1; i < max_consider; i++) {
            // Must be at least half as likely to be played.
            if (moves[i].first > best_score / 2.0f) {
                cumul_score += moves[i].first;
                viable_moves++;
            } else {
                break;
            }
        }
        float pick = Random::get_Rng()->randflt() * cumul_score;
        float cumul_find = 0.0f;
        for (size_t i = 0; i < max_consider; i++) {
            cumul_find += moves[i].first;
            if (pick < cumul_find) {
                bestmove = moves[i].second;
                // Move in the expected place
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }

    if (passflag & UCTSearch::NOPASS) {
        if (bestmove == FastBoard::PASS) {
            // Alternatives to passing?
            if (moves.size() > 0) {
                int altmove = moves[1].second;
                // The alternate move is a self-atari, check if
                // passing would actually win.
                if (m_rootstate.board.self_atari(color, altmove)) {
                    // score with no dead group removal
                    float score = m_rootstate.calculate_mc_score();
                    // passing would lose? play the self-atari
                    if ((score > 0.0f && color == FastBoard::WHITE)
                        ||
                        (score < 0.0f && color == FastBoard::BLACK)) {
                        myprintf("Passing loses, playing self-atari.\n");
                        bestmove = altmove;
                    } else {
                        myprintf("Passing wins, avoiding self-atari.\n");
                    }
                } else {
                    // alternatve move is not self-atari, so play it
                    bestmove = altmove;
                }
            } else {
                myprintf("Pass is the only acceptable move.\n");
            }
        }
    } else {
        // Check what happens if we pass twice
        if (m_rootstate.get_last_move() == FastBoard::PASS) {
            // score including dead stone removal
            float score = m_rootstate.final_score();
            // do we lose by passing?
            if ((score > 0.0f && color == FastBoard::WHITE)
                ||
                (score < 0.0f && color == FastBoard::BLACK)) {
                myprintf("Passing loses :-(\n");
                // We were going to pass, so try something else
                if (bestmove == FastBoard::PASS) {
                    if (moves.size() > 1) {
                        myprintf("Avoiding pass because it loses.\n");
                        bestmove = moves[1].second;
                    } else {
                        myprintf("No alternative to passing.\n");
                    }
                }
            } else {
                myprintf("Passing wins :-)\n");
                bestmove = FastBoard::PASS;
            }
        }
    }

    // if we aren't passing, should we consider resigning?
    if (bestmove != FastBoard::PASS) {
        // resigning allowed
        if ((passflag & UCTSearch::NORESIGN) == 0) {
            size_t movetresh = (m_rootstate.board.get_boardsize()
                                * m_rootstate.board.get_boardsize()) / 2;
            // bad score and zero wins in the playouts
            if (score <= 0.0f && m_rootstate.m_movenum > movetresh) {
                myprintf("All playouts lose. Resigning.\n");
                bestmove = FastBoard::RESIGN;
            }
        }
    }

    return bestmove;
}


int UCTSearch::get_best_move(passflag_t passflag) { 
    int color = m_rootstate.board.get_to_move();    

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
            size_t movetresh= (m_rootstate.board.get_boardsize()
                                * m_rootstate.board.get_boardsize()) / 3;
            // bad score and visited enough
            if (bestscore < 0.10f 
                && visits > 90 
                && m_rootstate.m_movenum > movetresh) {
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
    for (size_t i = 0; i < moves.size(); i++) {
        if (moves[i] > 0) {
            ord_list.push_back(std::make_pair(
                               m_rootstate.score_move(territory, moyo, moves[i]), 
                               m_rootstate.move_to_text(moves[i])));
        }
    } 
    
    std::stable_sort(ord_list.rbegin(), ord_list.rend());
    
    myprintf("\nOrder Table\n");
    myprintf("--------------------\n");
    for (size_t i = 0; i < std::min<size_t>(10, ord_list.size()); i++) {
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
    int color = tempstate.board.get_to_move();
                
    std::string pvstring = get_pv(tempstate, m_root);
    
    float winrate = m_root.get_winrate(color) * 100.0f;
    winrate = std::max(0.0f, winrate);
    winrate = std::min(100.0f, winrate);
        
    myprintf("Nodes: %d, Win: %5.2f%%, PV: %s\n", m_root.get_visits(), 
             winrate, pvstring.c_str());   

    if (!m_quiet) {
        GUIprintf("Nodes: %d, Win: %5.2f%%, PV: %s", m_root.get_visits(), 
                   winrate, pvstring.c_str());   
    } else {
        GUIprintf("%d nodes searched", m_root.get_visits());
    }
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
    m_rootstate.board.set_to_move(color);

    // set up timing info
    Time start;
    int time_for_move;


    if (!m_analyzing) {
        m_rootstate.get_timecontrol()->set_boardsize(m_rootstate.board.get_boardsize());
        time_for_move = m_rootstate.get_timecontrol()->max_time_for_move(color);       

        GUIprintf("Thinking at most %.1f seconds...", time_for_move/100.0f);

#ifdef USE_SEARCH
        // book moves
        if (m_rootstate.get_handicap() == 0) {
            if (m_rootstate.board.get_ko_hash() == 0xF64DA7066702B027ULL) {
                int rm = Random::get_Rng()->randint(2);
                if (rm == 0) {
                    return 60;  // e5
                } else if (rm == 1) {
                    return 71;  // e6
                }
            } else if (m_rootstate.board.get_ko_hash() == 0xF6760A0D837545EBULL) {
                if (Random::get_Rng()->randint(2) == 0) {
                    return 82; // e7
                } else {
                    return 81; // d7
                }
            } else if (m_rootstate.board.get_ko_hash() == 0x15D007FEE8E91047ULL) {
                int rm = Random::get_Rng()->randint(3);
                if (rm == 0) {
                    return 352; // r3
                } else if (rm == 1) {
                    return 373; // c4
                } else {
                    return 374; // d4
                }
            }
        }
#endif
    } else {
        time_for_move = INT_MAX;
                
        GUIprintf("Thinking...");
    }
    
    //XXX: testing    
    //int max_iterations = 10000;
                 
    m_rootstate.start_clock(color);   

    // do some preprocessing for move ordering
    MCOwnerTable::clear();
    float territory;
    float mc_score = Playout::mc_owner(m_rootstate, 64, &territory);

#ifdef USE_SEARCH
    // create a sorted list off legal moves (make sure we
    // play something legal and decent even in time trouble)
    m_nodes += m_root.create_children(m_rootstate, true);
    m_root.kill_superkos(m_rootstate);

    m_run = true;
#ifdef USE_SMP
    int cpus = SMP::get_num_cpus();
    //int cpus = 4;
    boost::thread_group tg;
    for (int i = 1; i < cpus; i++) {
        tg.create_thread(UCTWorker(m_rootstate, this, &m_root));
    }
#endif
    bool keeprunning = true;
    int iterations = 0;
    int last_update = 0;
    do {
        KoState currstate = m_rootstate;
                
        play_simulation(currstate, &m_root);                   

        Time elapsed;
        int centiseconds_elapsed = Time::timediff(start, elapsed);

        // output some stats every second
        // check if we should still search
        if (!m_analyzing) {
            if (centiseconds_elapsed - last_update > 250) {
                last_update = centiseconds_elapsed;                                                   
		dump_analysis();
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
        //if (iterations++ > max_iterations) {
        //    keeprunning = false;
        //}     
    } while(keeprunning);
    
    // stop the search
    m_run = false;
#ifdef USE_SMP
    tg.join_all();
#endif
    if (!m_root.has_children()) {
        return FastBoard::PASS;
    }
#else
    myprintf("MC winrate=%f score=", mc_score);
    if (territory > 0.0f) {
        myprintf("B+%3.1f\n", territory);
    } else {
        myprintf("W+%3.1f\n", -territory);
    }
    // Pure NN player
    // Not all net_moves vertices are legal
    auto net_moves = Network::get_Network()->get_scored_moves(&m_rootstate);
    auto gen_moves = m_rootstate.generate_moves(color);
    std::vector<std::pair<float, int>> filter_moves;

    // Legal moves minus superkos
    for(auto it = gen_moves.begin(); it != gen_moves.end(); ++it) {
        int move = *it;
        if (move != FastBoard::PASS) {
            // self atari, suicide, eyefill
            if (m_rootstate.try_move(color, move, true)) {
                KoState mystate = m_rootstate;
                mystate.play_move(move);
                if (!mystate.superko()) {
                    // "move" is now legal, find it
                    for (auto it_net = net_moves.begin();
                         it_net != net_moves.end(); ++it_net) {
                        if (it_net->second == move) {
                            filter_moves.push_back(std::make_pair(it_net->first, move));
                        }
                    }
                }
            }
        }
    }

    int stripped_moves = net_moves.size() - filter_moves.size();
    if (stripped_moves) {
        myprintf("Stripped %d illegal move(s).\n", stripped_moves);
    }
    // Add passing at a very low but non-zero weight
    filter_moves.push_back(std::make_pair(0.0001f, FastBoard::PASS));
#endif
    m_rootstate.stop_clock(color);

#ifdef USE_SEARCH
    // display search info        
    myprintf("\n");

    dump_stats(m_rootstate, m_root);

    int centiseconds_elapsed = Time::timediff(start, elapsed);
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
#else
    int bestmove = get_best_move_nosearch(filter_moves, mc_score, passflag);
#endif

    GUIprintf("Best move: %s", m_rootstate.move_to_text(bestmove).c_str());

    return bestmove;
}

void UCTSearch::ponder() {
    MCOwnerTable::clear();
    Playout::mc_owner(m_rootstate, 64);

#ifdef USE_SEARCH
    m_run = true;
#ifdef USE_SMP
    int cpus = SMP::get_num_cpus();
    //int cpus = 4;     
    boost::thread_group tg;
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
#ifdef USE_SMP
    tg.join_all();
#endif
    // display search info        
    myprintf("\n");
    dump_stats(m_rootstate, m_root);                  
               
    myprintf("\n%d visits, %d nodes\n\n", m_root.get_visits(), m_nodes);
#endif
}

void UCTSearch::set_visit_limit(int visits) {
    if (visits == 0) {
        m_maxvisits = INT_MAX;
    } else {
        m_maxvisits = visits;
    }
}

void UCTSearch::set_analyzing(bool flag) {
    m_analyzing = flag;
}

void UCTSearch::set_quiet(bool flag) {
    m_quiet = flag;
}
