#include "config.h"

#include <assert.h>
#include <limits.h>
#include <cmath>
#include <vector>
#include <utility>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#include <boost/thread.hpp>
#include <boost/thread/detail/tss_hooks.hpp>
#endif

#include "FastBoard.h"
#include "UCTSearch.h"
#include "Timing.h"
#include "Random.h"
#include "Utils.h"
#include "TTable.h"
#include "MCOTable.h"
#include "Network.h"
#include "GTP.h"
#include "Book.h"
#ifdef USE_OPENCL
#include "OpenCL.h"
#endif

using namespace Utils;

UCTSearch::UCTSearch(GameState & g)
    : m_rootstate(g),
      m_root(FastBoard::PASS, 0.0f, 1, 1, g.board.get_stone_count()),
      m_nodes(0),
      m_playouts(0),
      m_hasrunflag(false),
      m_runflag(NULL),
      m_analyzing(false),
      m_quiet(false) {
    set_use_nets(cfg_enable_nets);
    set_playout_limit(cfg_max_playouts);
    if (m_use_nets) {
        cfg_beta = 9.5f;
        cfg_patternbonus = 0.00075f;
    } else {
        cfg_uct = 0.00025f;
        cfg_mcts_fpu = 1.3f;
        cfg_beta = 32.0f;
        cfg_patternbonus = 0.0035f;
    }
}

void UCTSearch::set_runflag(std::atomic<bool> * flag) {
    m_runflag = flag;
    m_hasrunflag = true;
}

void UCTSearch::set_use_nets(bool flag) {
    m_use_nets = flag;
    if (m_rootstate.board.get_boardsize() != 19) {
        m_use_nets = false;
    }
}

Playout UCTSearch::play_simulation(KoState & currstate, UCTNode* const node) {
    const int color = currstate.get_to_move();
    const uint64 hash = currstate.board.get_hash();
    Playout noderesult;
    bool update_eval = true;

    TTable::get_TT()->sync(hash, node);

    if (m_use_nets
        && !node->get_evalcount()
        && node->get_visits() > cfg_eval_thresh) {
        node->run_value_net(currstate);

        // Check whether we have new evals to back up
        if (!node->has_eval_propagated() && node->get_evalcount()) {
            SMP::Lock lock(node->get_mutex());
            if (!node->has_eval_propagated()) {
                noderesult.set_eval(node->get_blackevals());
                node->set_eval_propagated();
                // Don't accumulate our own eval twice
                update_eval = false;
            }
        }
    }

    if (!node->has_children()
        && node->should_expand()
        && m_nodes < MAX_TREE_SIZE) {
        node->create_children(m_nodes, currstate, false, m_use_nets);
    }
    // This can happen at the same time as the previous one if this
    // position comes from the TTable.
    if (m_use_nets
        && node->has_children()
        && node->should_netscore()) {
        node->netscore_children(m_nodes, currstate, false);
    }

    if (node->has_children()) {
        UCTNode * next = node->uct_select_child(color, m_use_nets);

        if (next != NULL) {
            int move = next->get_move();

            if (move != FastBoard::PASS) {
                currstate.play_move(move);

                if (!currstate.superko()) {
                    noderesult = play_simulation(currstate, next);
                } else {
                    next->invalidate();
                    noderesult.run(currstate, false, true);
                }
            } else {
                currstate.play_pass();
                noderesult = play_simulation(currstate, next);
            }
        } else {
            noderesult.run(currstate, false, true);
        }
        node->updateRAVE(noderesult, color);
    } else {
        noderesult.run(currstate, false, true);
    }

    node->update(noderesult, color, update_eval);
    TTable::get_TT()->update(hash, node);

    return noderesult;
}

void UCTSearch::dump_GUI_stats(GameState & state, UCTNode & parent) {
#ifndef _CONSOLE
    const int color = state.get_to_move();

    if (!parent.has_children()) {
        return;
    }

    // sort children, put best move on top
    m_root.sort_root_children(color);

    UCTNode * bestnode = parent.get_first_child();
    if (bestnode->first_visit()) {
        return;
    }

    int total_visits = 0;
    UCTNode * node = bestnode;
    while (node != nullptr) {
        total_visits += node->get_visits();
        node = node->get_sibling();
    }

    using TRowVector = std::vector<std::pair<std::string, std::string>>;
    using TDataVector = std::vector<TRowVector>;

    auto analysis_data = new TDataVector();
    auto move_data = new std::vector<std::pair<std::string, float>>();
    node = bestnode;
    int movecount = 0;
    while (node != nullptr) {
        if (node->get_score() > 0.005f || node->get_visits() > 0) {
            std::string movestr = state.move_to_text(node->get_move());
            std::string pvstring(movestr);

            GameState tmpstate = state;

            tmpstate.play_move(node->get_move());
            pvstring += " " + get_pv(tmpstate, *node);

            std::vector<std::pair<std::string, std::string>> row;
            row.push_back(std::make_pair(std::string("Move"), movestr));
            row.push_back(std::make_pair(std::string("Effort%"),
                std::to_string(100.0 * node->get_visits() / (double)total_visits)));
            row.push_back(std::make_pair(std::string("Simulations"),
                std::to_string(node->get_visits())));
            row.push_back(std::make_pair(std::string("Win%"),
                node->get_visits() > 0 ?
                    std::to_string(node->get_mixed_score(color)*100.0f) :
                    std::string("-")));
            if (m_use_nets) {
                row.push_back(std::make_pair(std::string("MC Win%"),
                    node->get_visits() > 0 ?
                    std::to_string(node->get_winrate(color)*100.0f) :
                    std::string("-")));
                row.push_back(std::make_pair(std::string("Net Win%"),
                    node->get_evalcount() > 0 ?
                    std::to_string(node->get_eval(color)*100.0f) :
                    std::string("-")));
            }
            row.push_back(std::make_pair(
                m_use_nets ? std::string("Net Prob%") : std::string("Eval"),
                std::to_string(node->get_score() * 100.0f)));
            row.push_back(std::make_pair(std::string("PV"), pvstring));

            analysis_data->push_back(row);

            move_data->push_back(
                std::make_pair(movestr,
                               (float)(node->get_visits() / (double)total_visits)));
        }

        node = node->get_sibling();
    }

    GUIAnalysis((void*)analysis_data);
    GUIBestMoves((void*)move_data);
#endif
}

void UCTSearch::dump_stats(KoState & state, UCTNode & parent) {
#ifdef _CONSOLE
    const int color = state.get_to_move();

    if (!parent.has_children()) {
        return;
    }

    // sort children, put best move on top
    m_root.sort_root_children(color);

    UCTNode * bestnode = parent.get_first_child();

    if (bestnode->first_visit()) {
        return;
    }

    int movecount = 0;
    UCTNode * node = bestnode;

    while (node != NULL) {
        if (++movecount > 6) break;

        std::string tmp = state.move_to_text(node->get_move());
        std::string pvstring(tmp);

        if (!m_use_nets) {
        myprintf("%4s -> %7d (U: %5.2f%%) (R: %5.2f%%: %7d) (N: %4.1f%%) PV: ",
                      tmp.c_str(),
                      node->get_visits(),
                      node->get_visits() > 0 ? node->get_winrate(color)*100.0f : 0.0f,
                      node->get_visits() > 0 ? node->get_raverate()*100.0f : 0.0f,
                      node->get_ravevisits(),
                      node->get_score() * 100.0f);
        } else {
            myprintf("%4s -> %7d (W: %5.2f%%) (U: %5.2f%%) (V: %5.2f%%: %6d) (N: %4.1f%%) PV: ",
                tmp.c_str(),
                node->get_visits(),
                node->get_mixed_score(color)*100.0f,
                node->get_visits() > 0 ? node->get_winrate(color)*100.0f : 0.0f,
                node->get_evalcount() > 0 ? node->get_eval(color)*100.0f : 0.0f,
                node->get_evalcount(),
                node->get_score() * 100.0f);
        }

        KoState tmpstate = state;

        tmpstate.play_move(node->get_move());
        pvstring += " " + get_pv(tmpstate, *node);

        myprintf("%s\n", pvstring.c_str());

        node = node->get_sibling();
    }

    std::string tmp = state.move_to_text(bestnode->get_move());
    myprintf("====================================\n"
             "%d visits, score %5.2f%% (from %5.2f%%) PV: ",
             bestnode->get_visits(),
             bestnode->get_visits() > 0 ? bestnode->get_mixed_score(color)*100.0f : 0.0f,
             parent.get_mixed_score(color) * 100.0f,
             tmp.c_str());

    KoState tmpstate = state;
    myprintf(get_pv(tmpstate, parent).c_str());

    myprintf("\n");
#endif
}

bool UCTSearch::easy_move_precondition() {
   if (!m_use_nets) {
        return false;
    }
    if (!m_root.has_children()) {
        return false;
    }

    float best_probability = 0.0f;

    // do we have statistics on the moves?
    UCTNode * first = m_root.get_first_child();
    if (first != NULL) {
        best_probability = first->get_score();
        if (best_probability < 0.60f) {
            return false;
        }
    } else {
        return false;
    }

    UCTNode * second = first->get_sibling();
    if (second != NULL) {
        float second_probability = second->get_score();
        if (second_probability * 10.0f < best_probability) {
            return true;
        }
    } else {
        return true;
    }
    return false;
}

bool UCTSearch::allow_easy_move() {
    // Precondition failure should mean we don't get here.
    // We can assume there are at least 2 moves now.
    assert(m_use_nets);

    int color = m_rootstate.board.get_to_move();
    m_root.sort_root_children(color);

    UCTNode * first = m_root.get_first_child();
    float best_probability = first->get_score();
    // Some other move got to first place.
    if (best_probability < 0.60f) {
        return false;
    }

    UCTNode * second = first->get_sibling();
    float second_probability = second->get_score();
    if (second_probability * 10.0f < best_probability) {
        myprintf("Allowing very early exit: score: %5.2f%% >> %5.2f%%\n",
                 best_probability * 100.0f, second_probability*100.0f);
        return true;
    }

    return false;
}

bool UCTSearch::allow_early_exit() {
    if (!m_root.has_children()) {
        return false;
    }

    int color = m_rootstate.board.get_to_move();
    m_root.sort_root_children(color);

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
            // Stil not visited? Seems unlikely to happen then.
            return true;
        }
    } else {
        // We have a first move, but no sibling, after
        // already searching half the time
        return true;
    }

    double n1 = first->get_visits();
    double p1 = first->get_mixed_score(color);
    double n2 = second->get_visits();
    double p2 = second->get_mixed_score(color);
    double low, high;

    // Variance of Bernoulli distribution is p(1-p) = 0.25
    // Standard error is var/sqrt(n)
    // Thus standard error on MC = sqrt(0.25)/sqrt(n) = sqrt(0.25/n)
    low  = p1 - 3.0f * std::sqrt(0.25 / n1);
    high = p2 + 3.0f * std::sqrt(0.25 / n2);

    if (low > high) {
        myprintf("Allowing early exit: low: %f%% > high: %f%%\n", low * 100.0f, high * 100.0f);
        return true;
    }
    if (p1 < 0.10 || p1 > 0.95) {
        myprintf("Allowing early exit: score: %f%%\n", p1 * 100.0f);
        return true;
    }

    return false;
}

int UCTSearch::get_best_move_nosearch(std::vector<std::pair<float, int>> moves,
                                      float score, passflag_t passflag) {
    int color = m_rootstate.board.get_to_move();

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
    m_root.sort_root_children(color);

    int bestmove = m_root.get_first_child()->get_move();

    // do we have statistics on the moves?
    if (m_root.get_first_child() != NULL) {
        if (m_root.get_first_child()->first_visit()) {
            return bestmove;
        }
    }

    float bestscore = m_root.get_first_child()->get_winrate(color);

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
        // Opponents last move was passing
        if (m_rootstate.get_last_move() == FastBoard::PASS) {
            // We didn't consider passing. Should we have and
            // end the game immediately?
            float score = m_rootstate.final_score();
            // do we lose by passing?
            if ((score > 0.0f && color == FastBoard::WHITE)
                ||
                (score < 0.0f && color == FastBoard::BLACK)) {
                myprintf("Passing loses, I'll play on\n");
            } else {
                myprintf("Passing wins, I'll pass out\n");
                bestmove = FastBoard::PASS;
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

    float besteval = m_root.get_first_child()->get_eval(color);
    int visits = m_root.get_first_child()->get_visits();

    // if we aren't passing, should we consider resigning?
    if (bestmove != FastBoard::PASS) {
        // resigning allowed
        if ((passflag & UCTSearch::NORESIGN) == 0) {
            size_t movetresh= (m_rootstate.board.get_boardsize()
                                * m_rootstate.board.get_boardsize()) / 3;
            // bad score and visited enough
            if (((bestscore < 0.20f && besteval < 0.15f)
                 || (bestscore < 0.10f))
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

std::string UCTSearch::get_pv(KoState & state, UCTNode & parent) {
    if (!parent.has_children()) {
        return std::string();
    }

    // This breaks best probility = first in tree assumption
    parent.sort_root_children(state.get_to_move());

    UCTNode * bestchild = parent.get_first_child();
    int bestmove = bestchild->get_move();
    std::string tmp = state.move_to_text(bestmove);

    std::string res(tmp);
    res.append(" ");

    state.play_move(bestmove);

    std::string next = get_pv(state, *bestchild);
    res.append(next);

    // Resort according to move probability
    parent.sort_children();

    return res;
}

void UCTSearch::dump_analysis(void) {
    GameState tempstate = m_rootstate;
    int color = tempstate.board.get_to_move();

    std::string pvstring = get_pv(tempstate, m_root);
    float winrate = 100.0f * m_root.get_winrate(color);
    float mixrate = 100.0f * m_root.get_mixed_score(color);

    if (m_use_nets && m_root.get_evalcount()) {
        float eval = 100.0f * m_root.get_eval(color);
        myprintf("Nodes: %d, Win: %5.2f%% (MC:%5.2f%%/VN:%5.2f%%), PV: %s\n",
                 m_root.get_visits(),
                 mixrate, winrate, eval, pvstring.c_str());
    } else {
        myprintf("Nodes: %d, Win: %5.2f%%, PV: %s\n",
                 m_root.get_visits(), winrate, pvstring.c_str());
    }

    if (!m_quiet) {
        GUIprintf("Nodes: %d, Win: %5.2f%%, PV: %s", m_root.get_visits(),
                   mixrate, pvstring.c_str());
    } else {
        GUIprintf("%d nodes searched", m_root.get_visits());
    }
}

bool UCTSearch::is_running() {
    return m_run;
}

bool UCTSearch::playout_limit_reached() {
    return m_playouts >= m_maxplayouts;
}

void UCTWorker::operator()() {
#ifdef USE_OPENCL
    opencl.thread_init();
#endif
    do {
        KoState currstate = m_rootstate;
        m_search->play_simulation(currstate, m_root);
        m_search->increment_playouts();
    } while(m_search->is_running() && !m_search->playout_limit_reached());
#ifdef USE_OPENCL
    opencl.join_outstanding_cb();
#endif
#ifdef _WIN32
    boost::on_thread_exit();
#endif
}

std::tuple<float, float, float> UCTSearch::get_scores() {
    int color = m_rootstate.board.get_to_move();

    // make sure best is first
    m_root.sort_root_children(color);

    // do we have statistics on the moves?
    if (m_root.get_first_child() == nullptr) {
        return std::make_tuple(-1.0f, -1.0f, -1.0f);
    }

    UCTNode* bestnode = m_root.get_first_child();

    float bestmc =
       (bestnode->first_visit() ? -1.0f : bestnode->get_winrate(FastBoard::BLACK));
    float bestvn =
       (bestnode->get_evalcount() == 0 ? -1.0f : bestnode->get_eval(FastBoard::BLACK));
    float bestscore =
        ((bestnode->first_visit() || (bestnode->get_evalcount() == 0))
         ? -1.0f : bestnode->get_mixed_score(FastBoard::BLACK));

    return std::make_tuple(bestscore, bestmc, bestvn);
}

void UCTSearch::increment_playouts() {
    m_playouts++;
}

int UCTSearch::think(int color, passflag_t passflag) {
    // Start counting time for us
    m_rootstate.start_clock(color);

#ifdef USE_OPENCL
    opencl.thread_init();
#endif
    // set side to move
    m_rootstate.board.set_to_move(color);

    // set up timing info
    Time start;
    int time_for_move;

    if (!m_analyzing) {
        m_rootstate.get_timecontrol()->set_boardsize(m_rootstate.board.get_boardsize());
        time_for_move = m_rootstate.get_timecontrol()->max_time_for_move(color);

        GUIprintf("Thinking at most %.1f seconds...", time_for_move/100.0f);
#ifdef KGS
        if (m_rootstate.get_handicap() > 3
            || m_rootstate.get_komi() < 0.0f
            || m_rootstate.get_komi() > 8.0f) {
            myprintf("Bullshit game parameters, resigning...\n");
            return FastBoard::RESIGN;
        }
#endif
#ifdef USE_SEARCH
        if (m_rootstate.get_movenum() < 30) {
            int bookmove = Book::get_book_move(m_rootstate);
            if (bookmove != FastBoard::PASS) {
                return bookmove;
            }
        }
#endif
    } else {
        time_for_move = INT_MAX;
        GUIprintf("Thinking...");
    }

    // do some preprocessing for move ordering
    MCOwnerTable::clear();
    float territory;
    float mc_score = Playout::mc_owner(m_rootstate, 64, &territory);
    if (m_use_nets) {
        float net_score = Network::get_Network()->get_value(&m_rootstate,
                                                            Network::Ensemble::AVERAGE_ALL);
        myprintf("MC winrate=%f, NN eval=%f, score=", mc_score, net_score);
    } else {
         myprintf("MC winrate=%f, score=", mc_score);
    }
    if (territory > 0.0f) {
        myprintf("B+%3.1f\n", territory);
    } else {
        myprintf("W+%3.1f\n", -territory);
    }

#ifdef USE_SEARCH
    // create a sorted list off legal moves (make sure we
    // play something legal and decent even in time trouble)
    m_root.create_children(m_nodes, m_rootstate, true, m_use_nets);
    m_root.netscore_children(m_nodes, m_rootstate, true);
    m_root.kill_superkos(m_rootstate);

    m_run = true;
    m_playouts = 0;

    int cpus = cfg_num_threads;
    std::vector<std::thread> tg;
    for (int i = 1; i < cpus; i++) {
        tg.emplace_back(UCTWorker(m_rootstate, this, &m_root));
    }

    // If easy move precondition doesn't hold, pretend we
    // checked (and failed).
    bool easy_move_tested = !easy_move_precondition();
    bool keeprunning = true;
    int last_update = 0;
    do {
        KoState currstate = m_rootstate;

        play_simulation(currstate, &m_root);
        increment_playouts();

        Time elapsed;
        int centiseconds_elapsed = Time::timediff(start, elapsed);

        // output some stats every second
        // check if we should still search
        if (!m_analyzing) {
            if (centiseconds_elapsed - last_update > 250) {
                last_update = centiseconds_elapsed;
                dump_analysis();
                dump_GUI_stats(m_rootstate, m_root);
            }
            keeprunning = (centiseconds_elapsed < time_for_move
                           && (!m_hasrunflag || (*m_runflag)));
            keeprunning &= !playout_limit_reached();

            // check for early exit
            if (keeprunning && ((m_playouts & 127) == 0)) {
                if (centiseconds_elapsed > time_for_move/5) {
                    if (!easy_move_tested) {
                        keeprunning &= !allow_easy_move();
                        easy_move_tested = true;
                    }
                    if (centiseconds_elapsed > time_for_move/3) {
                         keeprunning &= !allow_early_exit();
                    }
                }
            }
        } else {
            if (centiseconds_elapsed - last_update > 100) {
                last_update = centiseconds_elapsed;
                dump_analysis();
                dump_GUI_stats(m_rootstate, m_root);
            }
            keeprunning = (!m_hasrunflag || (*m_runflag));
        }
    } while(keeprunning);

    // stop the search
    m_run = false;
#ifdef USE_OPENCL
    opencl.join_outstanding_cb();
#endif
    for (auto& thread : tg) {
        assert(thread.joinable());
        if (thread.joinable()) {
            thread.join();
        }
    }
    if (!m_root.has_children()) {
        return FastBoard::PASS;
    }
#else
    // Pure NN player
    // Not all net_moves vertices are legal
    auto net_moves = Network::get_Network()->get_scored_moves(&m_rootstate,
        Network::AVERAGE_ALL);
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
    filter_moves.push_back(std::make_pair(0.0001f, +FastBoard::PASS));
    std::stable_sort(filter_moves.rbegin(), filter_moves.rend());
#endif
    m_rootstate.stop_clock(color);

#ifdef USE_SEARCH
    // display search info
    myprintf("\n");

    dump_stats(m_rootstate, m_root);
    dump_GUI_stats(m_rootstate, m_root);

    Time elapsed;
    int centiseconds_elapsed = Time::timediff(start, elapsed);
    if (centiseconds_elapsed > 0) {
        myprintf("\n%d visits, %d nodes, %d playouts, %d p/s\n\n",
                 m_root.get_visits(),
                 (int)m_nodes,
                 (int)m_playouts,
                 (m_playouts * 100) / (centiseconds_elapsed+1));
        GUIprintf("%d visits, %d nodes, %d playouts, %d p/s",
                 m_root.get_visits(),
                  (int)m_nodes,
                  (int)m_playouts,
                 (m_playouts * 100) / (centiseconds_elapsed+1));
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
    m_playouts = 0;
    int cpus = cfg_num_threads;
    std::vector<std::thread> tg;
    for (int i = 1; i < cpus; i++) {
        tg.emplace_back(UCTWorker(m_rootstate, this, &m_root));
    }
    do {
        KoState currstate = m_rootstate;
        play_simulation(currstate, &m_root);
        increment_playouts();
    } while(!Utils::input_pending() && (!m_hasrunflag || (*m_runflag)));

    // stop the search
    m_run = false;
#ifdef USE_OPENCL
    opencl.join_outstanding_cb();
#endif
    for (auto& thread : tg) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    // display search info
    myprintf("\n");
    dump_stats(m_rootstate, m_root);
    dump_GUI_stats(m_rootstate, m_root);

    myprintf("\n%d visits, %d nodes\n\n", m_root.get_visits(), (int)m_nodes);
#endif
}

void UCTSearch::set_playout_limit(int playouts) {
    if (playouts == 0) {
        m_maxplayouts = INT_MAX;
    } else {
        m_maxplayouts = playouts;
    }
}

void UCTSearch::set_analyzing(bool flag) {
    m_analyzing = flag;
}

void UCTSearch::set_quiet(bool flag) {
    m_quiet = flag;
}
