#include <assert.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>

#include "FastBoard.h"
#include "FastState.h"
#include "Random.h"
#include "Utils.h"
#include "Playout.h"
#include "Zobrist.h"
#include "AttribScores.h"
#include "MCOTable.h"
#include "Genetic.h"
#include "GTP.h"
#include "MCPolicy.h"

using namespace Utils;

void FastState::init_game(int size, float komi) {
    board.reset_board(size);

    m_movenum = 0;

    m_komove = 0;
    m_lastmove = 0;
    m_last_was_capture = false;
    m_onebutlastmove = m_lastmove;
    m_komi = komi;
    m_handicap = 0;
    m_passes = 0;

    return;
}

void FastState::set_komi(float komi) {
    m_komi = komi;
}

void FastState::reset_game(void) {
    reset_board();

    m_movenum = 0;
    m_passes = 0;
    m_handicap = 0;
    m_komove = 0;

    m_lastmove = 0;
    m_last_was_capture = false;
    m_onebutlastmove = m_lastmove;
}

void FastState::reset_board(void) {
    board.reset_board(board.get_boardsize());
}

std::vector<int> FastState::generate_moves(int color) {
    std::vector<int> result;

    result.reserve(board.m_empty_cnt);        

    for (int i = 0; i < board.m_empty_cnt; i++) {
        int vertex = board.m_empty[i];

        if (vertex != m_komove && !board.is_suicide(vertex, color)) {
            result.push_back(vertex);
        }                                
    }

    result.push_back(+FastBoard::PASS);

    return result;
}

bool FastState::try_move(int color, int vertex) {
    if (vertex != m_komove && board.no_eye_fill(vertex)) {
        if (!board.fast_ss_suicide(color, vertex)) {
            return true;
        }
    }

    return false;
}

int FastState::walk_empty_list(int color) {
    int dir = Random::get_Rng()->randfix<2>();
    int vidx = Random::get_Rng()->randint16(board.m_empty_cnt);

    if (dir == 0) {
        for (int i = vidx; i < board.m_empty_cnt; i++) {
            int e = board.m_empty[i];

            if (try_move(color, e)) {
                return e;
            }
        }
        for (int i = 0; i < vidx; i++) {
            int e = board.m_empty[i];

            if (try_move(color, e)) {
                return e;
            }
        }
    } else {
        for (int i = vidx; i >= 0; i--) {
            int e = board.m_empty[i];

            if (try_move(color, e)) {
                return e;
            }
        }
        for (int i = board.m_empty_cnt - 1; i > vidx; i--) {
            int e = board.m_empty[i];

            if (try_move(color, e)) {
                return e;
            }
        }
    }

    return FastBoard::PASS;
}

void FastState::flag_move(MovewFeatures & mwf, int sq, int color,
                          Matcher * matcher) {
    assert(sq > 0);
    int full_pattern = board.get_pattern_fast_augment(sq);
    int pattern = matcher->matches(color, full_pattern);
    mwf.set_pattern(pattern);
    //bool invert_board = false;
    //if (color == FastBoard::WHITE) {
    //    invert_board = true;
    //}
    //uint32 pattern = board.get_pattern3_augment(sq, invert_board);
    //mwf.set_pattern(Utils::pattern_hash(pattern));

    std::pair<int, int> nbr_crit = board.nbr_criticality(color, sq);

    if (nbr_crit.first == 1) {
        mwf.add_flag(MWF_FLAG_CRIT_MINE1);
    } else  if (nbr_crit.first == 2) {
        mwf.add_flag(MWF_FLAG_CRIT_MINE2);
    }  else  if (nbr_crit.first == 3) {
        mwf.add_flag(MWF_FLAG_CRIT_MINE3);
    }
    if (nbr_crit.second == 1) {
        mwf.add_flag(MWF_FLAG_CRIT_HIS1);
    } else if (nbr_crit.second == 2) {
        mwf.add_flag(MWF_FLAG_CRIT_HIS2);
    } else if (nbr_crit.second == 3) {
        mwf.add_flag(MWF_FLAG_CRIT_HIS3);
    }

    if (board.self_atari(color, sq)) {
        if (board.is_suicide(sq, color)) {
            mwf.add_flag(MWF_FLAG_SUICIDE);
        } else {
            // Self-atari with a group of 6 stones always leaves behind
            // a live group due to eyespace of size 7.
            if (board.enemy_atari_size(!color, sq) >= 6) {
                mwf.add_flag(MWF_FLAG_TOOBIG_SA);
            } else {
                int enemy_dying = board.enemy_atari_size(color, sq);
                if (enemy_dying >= 5) {
                    mwf.add_flag(MWF_FLAG_FORCEBIG_SA);
                } else if (enemy_dying) {
                    mwf.add_flag(MWF_FLAG_FORCE_SA);
                } else {
                    mwf.add_flag(MWF_FLAG_SA);
                }
            }
            if (mwf.has_flag(MWF_FLAG_PATTERN)) {
                mwf.add_flag(MWF_FLAG_PATTERN_SA);
            }
            if (mwf.has_flag(MWF_FLAG_SAVING)) {
                mwf.add_flag(MWF_FLAG_SAVING_SA);
            }
        }
    }

    if (mwf.has_flag(MWF_FLAG_SAVING)) {
        int ssize = mwf.get_target_size();
        if (ssize == 1) {
            mwf.add_flag(MWF_FLAG_SAVING_1);
        } else if (ssize == 2) {
            mwf.add_flag(MWF_FLAG_SAVING_2);
        } else {
            assert(ssize >= 3);
            mwf.add_flag(MWF_FLAG_SAVING_3P);
        }
    }

    if (mwf.has_flag(MWF_FLAG_CAPTURE)) {
        int ssize = mwf.get_target_size();
        if (ssize == 1) {
            mwf.add_flag(MWF_FLAG_CAPTURE_1);
        } else if (ssize == 2) {
            mwf.add_flag(MWF_FLAG_CAPTURE_2);
        } else {
            assert(ssize >= 3);
            mwf.add_flag(MWF_FLAG_CAPTURE_3P);
        }
    }
}

int FastState::play_random_move(int color, PolicyTrace * trace) {
    board.m_tomove = color;

    moves.clear();

    Matcher * matcher = Matcher::get_Matcher();
    Random * rng = Random::get_Rng();

    // Local moves, or tactical ones
    if (m_lastmove > 0 && m_lastmove < board.m_maxsq) {
        if (board.get_square(m_lastmove) == !color) {
            board.add_global_captures(color, moves);
            board.save_critical_neighbours(color, m_lastmove, moves);
            if (m_last_was_capture || (0.3f > rng->randflt())) {
                board.add_near_nakade_moves(color, m_lastmove, moves);
            }
            board.add_pattern_moves(color, m_lastmove, moves);
        }
    }

    constexpr int loop_amount = 4;
    // Random moves on the board
    for (int loops = 0; loops < loop_amount; loops++) {
        int sq = walk_empty_list(board.m_tomove);
        if (sq == FastBoard::PASS) {
            break;
        }
        moves.emplace_back(sq, MWF_FLAG_RANDOM);
    }

    moves.erase(std::remove_if(moves.begin(), moves.end(),
                               [this](MovewFeatures & mwf) {
                                   int sq = mwf.get_sq();
                                   assert(sq > 0);
                                   return sq == m_komove;
                               }),
                moves.end());

    // Pass as fallback
    moves.emplace_back(FastBoard::PASS, MWF_FLAG_PASS);
    assert(moves.size());

    float cumul = 0.0f;
    scoredmoves.clear();
    for (auto & mwf : moves) {
        int sq = mwf.get_sq();
        if (sq != FastBoard::PASS) {
            flag_move(mwf, sq, color, matcher);
        }
        cumul += mwf.get_score();
        scoredmoves.emplace_back(sq, cumul);
    }

    float index = rng->randflt() * cumul;

    for (size_t i = 0; i < scoredmoves.size(); i++) {
        float point = scoredmoves[i].second;
        if (index <= point) {
            if (trace) {
                trace->add_to_trace(color == FastBoard::BLACK, moves, i);
            }
            assert(scoredmoves[i].first == moves[i].get_sq());
            return play_move_fast(scoredmoves[i].first);
        }
    }

    assert(false);
    return play_move_fast(FastBoard::PASS);
}

float FastState::score_move(std::vector<int> & territory, std::vector<int> & moyo, int vertex) {       
    Attributes att;
    att.get_from_move(this, territory, moyo, vertex);

    return AttribScores::get_attribscores()->team_strength(att);        
}

int FastState::play_move_fast(int vertex) {
    bool capture = false;
    if (vertex == FastBoard::PASS) {
        increment_passes();
    } else {
        m_komove = board.update_board_fast(board.m_tomove, vertex, capture);
        set_passes(0);
    }

    m_onebutlastmove = m_lastmove;
    m_last_was_capture = capture;
    m_lastmove = vertex;
    board.m_tomove = !board.m_tomove;
    m_movenum++;

    return vertex;
}

void FastState::play_pass(void) {
    m_movenum++;

    m_onebutlastmove = m_lastmove;
    m_last_was_capture = false;
    m_lastmove = FastBoard::PASS;

    board.hash  ^= 0xABCDABCDABCDABCDULL;
    board.m_tomove = !board.m_tomove;

    board.hash ^= Zobrist::zobrist_pass[get_passes()];
    increment_passes();
    board.hash ^= Zobrist::zobrist_pass[get_passes()];
}

void FastState::play_move(int vertex) {
    play_move(board.m_tomove, vertex);
}

void FastState::play_move(int color, int vertex) {
    if (vertex != FastBoard::PASS && vertex != FastBoard::RESIGN) {
        bool capture = false;
        int kosq = board.update_board(color, vertex, capture);

        m_komove = kosq;
        m_onebutlastmove = m_lastmove;
        m_last_was_capture = capture;
        m_lastmove = vertex;

        m_movenum++;

        if (board.m_tomove == color) {
            board.hash  ^= 0xABCDABCDABCDABCDULL;
        }
        board.m_tomove = !color;

        if (get_passes() > 0) {
            board.hash ^= Zobrist::zobrist_pass[get_passes()];
            set_passes(0);
            board.hash ^= Zobrist::zobrist_pass[0];
        }
    } else {
        play_pass();
    }
}

int FastState::get_movenum() {
    return m_movenum;
}

int FastState::estimate_mc_score(void) {
    return board.estimate_mc_score(m_komi + m_handicap);
}

float FastState::calculate_mc_score(void) {
    return board.final_mc_score(m_komi + m_handicap);
}

int FastState::get_last_move(void) {
    return m_lastmove;
}

int FastState::get_prevlast_move() {
    return m_onebutlastmove;
}

int FastState::get_passes() {     
    return m_passes; 
}

void FastState::set_passes(int val) { 
    m_passes = val; 
}

void FastState::increment_passes() { 
    m_passes++;
    if (m_passes > 4) m_passes = 4;
}

int FastState::get_to_move() {
    return board.m_tomove;
}

void FastState::set_to_move(int tom) {
    board.m_tomove = tom;
}

void FastState::display_state() {        
    myprintf("\nPasses: %d            Black (X) Prisoners: %d\n", 
             m_passes, board.get_prisoners(FastBoard::BLACK));
    if (board.black_to_move()) {
        myprintf("Black (X) to move");
    } else {
        myprintf("White (O) to move");
    }
    myprintf("    White (O) Prisoners: %d\n", board.get_prisoners(FastBoard::WHITE));              

    board.display_board(m_lastmove);
}

std::string FastState::move_to_text(int move) {
    return board.move_to_text(move);
}

std::vector<bool> FastState::mark_dead(float *winrate) {
    static const int MARKING_RUNS = 256;
    
    std::vector<int> survive_count(FastBoard::MAXSQ);
    std::vector<bool> dead_group(FastBoard::MAXSQ);    
    
    fill(survive_count.begin(), survive_count.end(), 0);    
    fill(dead_group.begin(), dead_group.end(), false);

    float wins = 0.0;
    for (int i = 0; i < MARKING_RUNS; i++) {
        FastState workstate(*this);
        Playout p;

        p.run(workstate, true, false);

        float score = p.get_score();
        if (score > 0.0f) {
            wins += 1.0f;
        } else if (score == 0.0f) {
            wins += 0.5f;
        }

        for (int i = 0; i < board.get_boardsize(); i++) {
            for (int j = 0; j < board.get_boardsize(); j++) {
                int vertex = board.get_vertex(i, j);
                int sq    =           board.get_square(vertex);
                int mc_sq = workstate.board.get_square(vertex);

                if (sq == mc_sq) {
                    survive_count[vertex]++;
                }
            }
        }
    }

    const int LIVE_TRESHOLD = MARKING_RUNS / 2;
    if (winrate) {
        *winrate = wins / (float)MARKING_RUNS;
    }

    for (int i = 0; i < board.get_boardsize(); i++) {
        for (int j = 0; j < board.get_boardsize(); j++) {
            int vertex = board.get_vertex(i, j);

            if (survive_count[vertex] < LIVE_TRESHOLD) {
                dead_group[vertex] = true;
            }
        }
    }

    return dead_group;
}

std::vector<int> FastState::final_score_map() {
    FastState workstate(*this);    
                 
    std::vector<bool> dead_group = workstate.mark_dead();

     for (int i = 0; i < workstate.board.get_boardsize(); i++) {
        for (int j = 0; j < workstate.board.get_boardsize(); j++) {
            int vertex = workstate.board.get_vertex(i, j);
            
            if (dead_group[vertex]) {
                workstate.board.set_square(vertex, FastBoard::EMPTY);
            }
        }
    }

    std::vector<bool> white = workstate.board.calc_reach_color(FastBoard::WHITE);
    std::vector<bool> black = workstate.board.calc_reach_color(FastBoard::BLACK);
    
    std::vector<int> res;
    res.resize(FastBoard::MAXSQ);
    std::fill(res.begin(), res.end(), FastBoard::EMPTY);    
            
    for (int i = 0; i < workstate.board.get_boardsize(); i++) {
        for (int j = 0; j < workstate.board.get_boardsize(); j++) {
            int vertex = workstate.board.get_vertex(i, j);
            
            if (white[vertex] && !black[vertex]) {
                res[vertex] = FastBoard::WHITE;
            } else if (black[vertex] && !white[vertex]) {
                res[vertex] = FastBoard::BLACK;
            }
        }
    }
    
    return res;
}

float FastState::final_score(float *winrate) {
    FastState workstate(*this);

    std::vector<bool> dead_group = workstate.mark_dead(winrate);

     for (int i = 0; i < workstate.board.get_boardsize(); i++) {
        for (int j = 0; j < workstate.board.get_boardsize(); j++) {
            int vertex = workstate.board.get_vertex(i, j);

            if (dead_group[vertex]) {
                workstate.board.set_square(vertex, FastBoard::EMPTY);
            }
        }
    }

    return workstate.board.area_score(get_komi() + get_handicap());
}

float FastState::get_komi() {
    return m_komi;
}

void FastState::set_handicap(int hcap) {
    m_handicap = hcap;
}

int FastState::get_handicap() {
    return m_handicap;
}

int FastState::get_komove() {
    return m_komove;
}
