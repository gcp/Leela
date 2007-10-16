#include <assert.h>
#include <vector>
#include <algorithm>

#include "FastBoard.h"
#include "FastState.h"
#include "Random.h"
#include "Utils.h"
#include "Playout.h"
#include "Zobrist.h"
#include "Matcher.h"
#include "AttribScores.h"

using namespace Utils;

void FastState::init_game(int size, float komi) {
    
    board.reset_board(size);
    
    movenum = 0;                          
    
    lastmove = FastBoard::PASS;
    onebutlastmove = lastmove;
    m_komi = komi;
    m_passes = 0;
    
    return;
}

void FastState::set_komi(float komi) {
    m_komi = komi;
}

void FastState::reset_game(void) {
    reset_board();

    movenum = 0;   
    m_passes = 0;                       
    
    lastmove = FastBoard::MAXSQ;
    onebutlastmove = lastmove;
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

        if (vertex != komove && !board.is_suicide(vertex, color)) {   
            result.push_back(vertex);
        }                                
    }

    result.push_back(FastBoard::PASS);

    return result;
}

bool FastState::try_move(int color, int vertex) {    
    if (vertex != komove && board.no_eye_fill(vertex)) {
        if (!board.fast_ss_suicide(color, vertex)) {
            if (!board.self_atari(color, vertex)) {
                return true;
            }
        } 
    }                       
    
    return false;
}

int FastState::walk_empty_list(int color, int vidx) {
    int dir = Random::get_Rng()->randint(2);    

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

int FastState::play_random_move(int color) {                            
    board.m_tomove = color;                                         
    
    m_work.clear();    
    
    if (lastmove > 0 && lastmove < board.m_maxsq) {
        if (board.get_square(lastmove) == !color) {            
            board.add_global_captures(color, m_work);            
            if (m_work.empty()) {                
                board.save_critical_neighbours(color, lastmove, m_work);
            }            
            if (m_work.empty()) {                
                board.add_pattern_moves(color, lastmove, m_work);            
            }            
            // remove ko captures     
            m_work.erase(std::remove(m_work.begin(), m_work.end(), komove), m_work.end());                                           
        }        
    }                   
    
    int vidx;     
    int vtx;
                
    if (m_work.empty()) {         
        vidx = Random::get_Rng()->randint(board.m_empty_cnt); 
        vtx  = walk_empty_list(color, vidx); 
    } else {        
        if (m_work.size() > 1) {            
            // remove multiple moves    
            std::sort(m_work.begin(), m_work.end());    
            m_work.erase(std::unique(m_work.begin(), m_work.end()), m_work.end()); 
                       
            int idx = Random::get_Rng()->randint((uint16)m_work.size()); 
                        
            vtx = m_work[idx]; 
        } else {            
            vtx = m_work[0];
        }
    }            
              
    return play_move_fast(vtx);                         
}


float FastState::score_move(int vertex) {       
    Attributes att;
    att.get_from_move(this, vertex);

    return AttribScores::get_attribscores()->team_strength(att);        
}

int FastState::play_move_fast(int vertex) {
    if (vertex == FastBoard::PASS) {                       
        increment_passes();                 
    } else {                                                      
        komove = board.update_board_fast(board.m_tomove, vertex);        
        set_passes(0);                                            
    }
    
    onebutlastmove = lastmove;
    lastmove = vertex;                                              
    board.m_tomove = !board.m_tomove;
    movenum++; 
    
    return vertex;
}
   
void FastState::play_pass(void) {
    movenum++;
        
    onebutlastmove = lastmove;    
    lastmove = FastBoard::PASS;
        
    board.hash  ^= 0xABCDABCDABCDABCDUI64;    
    board.m_tomove = !board.m_tomove;                 
        
    board.hash ^= Zobrist::zobrist_pass[get_passes()];
    increment_passes();
    board.hash ^= Zobrist::zobrist_pass[get_passes()];              
}

void FastState::play_move(int vertex) {
    play_move(board.m_tomove, vertex);
}

void FastState::play_move(int color, int vertex) {
    if (vertex != FastBoard::PASS) {                   
        int kosq = board.update_board(color, vertex);
    
        komove = kosq;   
        onebutlastmove = lastmove;
        lastmove = vertex;
    
        movenum++;
        
        if (board.m_tomove == color) {
            board.hash  ^= 0xABCDABCDABCDABCDUI64;
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
    return movenum;
}

int FastState::estimate_mc_score(void) {
    return board.estimate_mc_score(m_komi);
}

float FastState::calculate_mc_score(void) {
    return board.final_mc_score(m_komi);
}

int FastState::get_last_move(void) {
    return lastmove;
}

int FastState::get_prevlast_move() {
    return onebutlastmove;
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

void FastState::display_state() {    
    myprintf("\nPasses: %d            Black (X) Prisoners: %d\n", 
             m_passes, board.get_prisoners(FastBoard::BLACK));
    if (board.black_to_move()) {
        myprintf("Black (X) to move");
    } else {
        myprintf("White (O) to move");
    }
    myprintf("    White (O) Prisoners: %d\n", board.get_prisoners(FastBoard::WHITE));              

    board.display_board(lastmove);
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
        
        p.run(workstate, false);
        
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
    
    const int LIVE_TRESHOLD = MARKING_RUNS / 5;
    
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
    
    return workstate.board.area_score(get_komi());
}

float FastState::get_komi() {
    return m_komi;
}