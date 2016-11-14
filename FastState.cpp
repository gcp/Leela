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
#include "Matcher.h"
#include "AttribScores.h"
#include "MCOTable.h"
#include "Genetic.h"
#include "GTP.h"

using namespace Utils;

void FastState::init_game(int size, float komi) {

    board.reset_board(size);

    m_movenum = 0;

    m_komove = 0;
    m_lastmove = 0;
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
    m_onebutlastmove = m_lastmove;
}

void FastState::reset_board(void) {
    board.reset_board(board.get_boardsize());
}

int FastState::play_random_move() {
    return play_random_move(board.m_tomove);
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

bool FastState::try_move(int color, int vertex, bool allow_sa) {
    if (vertex != m_komove && board.no_eye_fill(vertex)) {
        if (!board.fast_ss_suicide(color, vertex)) {
            if ((allow_sa) || (!board.self_atari(color, vertex))) {
                return true;
            }
        }
    }

    return false;
}

int FastState::walk_empty_list(int color, bool allow_sa) {
    int dir = Random::get_Rng()->randint(2);
    int vidx = Random::get_Rng()->randint(board.m_empty_cnt);

    if (dir == 0) {
        for (int i = vidx; i < board.m_empty_cnt; i++) {
            int e = board.m_empty[i];

            if (try_move(color, e, allow_sa)) {
                return e;
            }
        }
        for (int i = 0; i < vidx; i++) {
            int e = board.m_empty[i];

            if (try_move(color, e, allow_sa)) {
                return e;
            }
        }
    } else {
        for (int i = vidx; i >= 0; i--) {
            int e = board.m_empty[i];

            if (try_move(color, e, allow_sa)) {
                return e;
            }
        }
        for (int i = board.m_empty_cnt - 1; i > vidx; i--) {
            int e = board.m_empty[i];

            if (try_move(color, e, allow_sa)) {
                return e;
            }
        }
    }

    return FastBoard::PASS;
}

int FastState::select_weighted(FastBoard::scoredmoves_t & scoredmoves,
                                int cumul) {
    int index = Random::get_Rng()->randint32(cumul);

    for (size_t i = 0; i < scoredmoves.size(); i++) {
        int point = scoredmoves[i].second;
        if (index < point) {
            return play_move_fast(scoredmoves[i].first);
        }
    }

    return play_move_fast(FastBoard::PASS);
}

int FastState::select_uniform(FastBoard::movelist_t & moves) {
    if (moves.size()) {
        int index = Random::get_Rng()->randint32(moves.size());
        return play_move_fast(moves[index]);
    } else {
        return play_move_fast(FastBoard::PASS);
    }
}

int FastState::play_random_move(int color) {
    board.m_tomove = color;

    moves.clear();
    scoredmoves.clear();

    Matcher * matcher = Matcher::get_Matcher();
    Random * rng = Random::get_Rng();

    if (m_lastmove > 0 && m_lastmove < board.m_maxsq) {
        if (board.get_square(m_lastmove) == !color) {
            board.add_global_captures(color, moves);
            board.save_critical_neighbours(color, m_lastmove, moves);
            if (0.2f > rng->randflt()) {
                board.add_near_nakade_moves(color, m_lastmove, moves);
            }
            board.add_pattern_moves(color, m_lastmove, moves);
            moves.erase(std::remove_if(moves.begin(), moves.end(),
                                       [this](int sq){ return sq == m_komove;}),
                        moves.end());
        }
    }

    if (moves.size()) {
        float cumul = 0.0f;
        for (int sq : moves) {
            assert(sq != m_komove);
            int pattern = board.get_pattern_fast_augment(sq);
            float score = matcher->matches(color, pattern);
            std::pair<int, int> nbr_crit = board.nbr_criticality(color, sq);

            //static const std::array<int, 9> crit_mine = {
            //    1, 4, 1, 1, 1, 1, 1, 1, 1
            //};

            //static const std::array<int, 9> crit_enemy = {
            //    1, 14, 12, 1, 1, 1, 1, 1, 1
            //};

            // score *= crit_mine[nbr_crit.first];
            // score *= crit_enemy[nbr_crit.second];

            assert(nbr_crit.first != 0);
            assert(nbr_crit.second != 0);

            if (nbr_crit.first == 1) {
                score *= cfg_crit_mine_1;
            } else  if (nbr_crit.first == 2) {
                score *= cfg_crit_mine_2;
            }
            if (nbr_crit.second == 1) {
                score *= cfg_crit_his_1;
            } else if (nbr_crit.second == 2) {
                score *= cfg_crit_his_2;
            }

            bool nearby = false;
            for (int i = 0; i < 8; i++) {
                int ai = m_lastmove + board.get_extra_dir(i);
                if (ai == sq) {
                    nearby = true;
                    break;
                }
            }

            if (!nearby) {
                score *= cfg_tactical;
            }

            if (score >= cfg_bound) {
                cumul += score;
                scoredmoves.push_back(std::make_pair(sq, cumul));
            }
        }

        float index = rng->randflt() * cumul;

        for (size_t i = 0; i < scoredmoves.size(); i++) {
            float point = scoredmoves[i].second;
            if (index < point) {
                return play_move_fast(scoredmoves[i].first);
            }
        }
    }


    int loops = board.m_empty_cnt / 64;
    float cumul = 0.0f;

    do {
        int vtx = walk_empty_list(board.m_tomove, true);

        if (vtx == FastBoard::PASS) {
            return play_move_fast(FastBoard::PASS);
        }

        int pattern = board.get_pattern_fast_augment(vtx);
        float score = matcher->matches(color, pattern);

        if (board.self_atari(color, vtx)) {
            // Self-atari with a group of 6 stones always leaves behind
            // a live group due to eyespace of size 7.
            if (board.enemy_atari_size(!color, vtx) >= 6) {
                continue;
            }
            int enemy_dying = board.enemy_atari_size(color, vtx);
            if (enemy_dying) {
                score *= cfg_regular_self_atari;
            } else {
                score *= cfg_useless_self_atari;
            }
        }

        cumul += score;
        scoredmoves.push_back(std::make_pair(vtx, cumul));
    } while (--loops > 0);

    cumul += cfg_pass_score;
    scoredmoves.push_back(std::make_pair(FastBoard::PASS, cumul));

    float index = rng->randflt() * cumul;

    for (size_t i = 0; i < scoredmoves.size(); i++) {
        float point = scoredmoves[i].second;
        if (index < point) {
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
    if (vertex == FastBoard::PASS) {                       
        increment_passes();                 
    } else {                                                      
        m_komove = board.update_board_fast(board.m_tomove, vertex);
        set_passes(0);                                            
    }

    m_onebutlastmove = m_lastmove;
    m_lastmove = vertex;
    board.m_tomove = !board.m_tomove;
    m_movenum++;

    return vertex;
}

void FastState::play_pass(void) {
    m_movenum++;

    m_onebutlastmove = m_lastmove;
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
        int kosq = board.update_board(color, vertex);

        m_komove = kosq;
        m_onebutlastmove = m_lastmove;
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

std::vector<bool> FastState::mark_dead() {
    static const int MARKING_RUNS = 256;
    
    std::vector<int> survive_count(FastBoard::MAXSQ);    
    std::vector<bool> dead_group(FastBoard::MAXSQ);    
    
    fill(survive_count.begin(), survive_count.end(), 0);    
    fill(dead_group.begin(), dead_group.end(), false);        
    
    for (int i = 0; i < MARKING_RUNS; i++) {
        FastState workstate(*this);
        Playout p;
        
        p.run(workstate, true, false);
        
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

float FastState::final_score() {       
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
