#include <assert.h>
#include "FastBoard.h"
#include "FastState.h"
#include "Random.h"
#include "Utils.h"
#include "Playout.h"

using namespace Utils;

void FastState::init_game(int size, float komi) {
    
    board.reset_board(size);
    
    movenum = 0;                          
    
    lastmove = FastBoard::MAXSQ;
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
    
    lastmove = FastBoard::MAXSQ;
}

void FastState::reset_board(void) {
    board.reset_board(board.get_boardsize());
}

int FastState::play_random_move() {
    return play_random_move(board.m_tomove);
}

int FastState::play_random_move(int color) {                
    int vidx = Random::get_Rng()->randint(board.m_empty_cnt);         
    
    board.m_tomove = color;            
    
    /* alternate direction for less bias */
    for (int i = vidx; i < board.m_empty_cnt; i++) {  
        int vertex = board.m_empty[i];
        
        assert(board.get_square(vertex) == FullBoard::EMPTY);
                
        if (vertex != komove && board.no_eye_fill(vertex)) {
            if (!board.fast_ss_suicide(color, vertex)) {
                return play_move_fast(vertex);                
            } 
        }                   
    }  
    for (int i = 0; i < vidx; i++) {      
        int vertex = board.m_empty[i];
        
        assert(board.get_square(vertex) == FullBoard::EMPTY);
            
        if (vertex != komove && board.no_eye_fill(vertex)) {
            if (!board.fast_ss_suicide(color, vertex)) {
                return play_move_fast(vertex);                
            } 
        }       
    }              
              
    play_pass_fast();                
    return FastBoard::PASS;        
}


int FastState::play_move_fast(int vertex) {
    assert(vertex != FastBoard::PASS);
        
    int color = board.m_tomove;           
              
    int kosq = board.update_board_fast(color, vertex);        
        
    komove = kosq;        
    lastmove = vertex;
    
    movenum++;        
    board.m_tomove = !color;                
    set_passes(0);    
                
    return vertex;
}

int FastState::play_pass_fast(void) {    
    movenum++;            
                   
    lastmove = FastBoard::PASS;
    
    board.m_tomove = !board.m_tomove;                 
    increment_passes();
    
    return FastBoard::PASS;
}

int FastState::get_movenum() {
    return movenum;
}

int FastState::estimate_score(void) {
    return board.estimate_score(m_komi);
}

float FastState::calculate_mc_score(void) {
    return board.final_mc_score(m_komi);
}

int FastState::get_last_move(void) {
    return lastmove;
}

int FastState::get_passes() { 
    return m_passes; 
}

void FastState::set_passes(int val) { 
    m_passes = val; 
}

void FastState::increment_passes() { 
    set_passes(get_passes() + 1); 
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

void FastState::move_to_text(int move, char *vertex) {
    board.move_to_text(move, vertex);
}

std::vector<bool> FastState::mark_dead() {
    static const int MARKING_RUNS = 1000;
    
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
    
    return workstate.board.final_score(m_komi);
}