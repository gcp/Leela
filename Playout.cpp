#include <vector>
#include <boost/tr1/array.hpp>
#include "config.h"

#include "Timing.h"
#include "GameState.h"
#include "Playout.h"
#include "Utils.h"
#include "MCOTable.h"
#include "AMAFTable.h"

using namespace Utils;

Playout::Playout() {    
    m_run = false;
    m_sq[0].reset();
    m_sq[1].reset();
}

float Playout::get_score() {
    assert(m_run);
    assert(m_score > -2.00f && m_score < 2.00f);

    return m_score;
}

void Playout::set_final_score(float score) {
    m_run = true;
    m_score = score;
}

void Playout::run(FastState & state, bool resigning) {
    assert(!m_run);        

    const int boardsize = state.board.get_boardsize();
        
    const int resign = (boardsize * boardsize) / 3;
    const int playoutlen = (boardsize * boardsize) * 2;        
    
    int counter = 0; 
    
    // do the main loop        
    do {                                    
        int vtx = state.play_random_move();

        if (counter < 60 && vtx != FastBoard::PASS) {
            int color = !state.get_to_move();
                        
            if (!m_sq[!color][vtx]) {
                m_sq[color][vtx] = true;
            }                
        }           
        
        counter++;                 
    } while (state.get_passes() < 2 
             && state.get_movenum() < playoutlen
             && (!resigning || abs(state.estimate_mc_score()) < resign));     
             
    float score = state.calculate_mc_score();                                  
             
    // get ownership info             
    bitboard_t blackowns; 
    
    AMAFTable * amaft = AMAFTable::get_AMAFT();
    
    for (int i = 0; i < boardsize; i++) {
        for (int j = 0; j < boardsize; j++) {
            int vtx = state.board.get_vertex(i, j);            
            if (state.board.get_square(vtx) == FastBoard::BLACK) {
                blackowns[vtx] = true;
            } else if (state.board.get_square(vtx) == FastBoard::EMPTY) {
                if (state.board.is_eye(FastBoard::BLACK, vtx)) {
                    blackowns[vtx] = true;    
                }
            }    
            if (score > 0.0f) {
                if (m_sq[FastBoard::BLACK][vtx]) {
                    amaft->visit(FastBoard::BLACK, vtx, true);
                }
                if (m_sq[FastBoard::WHITE][vtx]) {
                    amaft->visit(FastBoard::WHITE, vtx, false);
                }
            } else {
                if (m_sq[FastBoard::BLACK][vtx]) {
                    amaft->visit(FastBoard::BLACK, vtx, false);
                }
                if (m_sq[FastBoard::WHITE][vtx]) {
                    amaft->visit(FastBoard::WHITE, vtx, true);
                }
            }        
        }
    }

    // update MCO in one swoop
    MCOwnerTable::get_MCO()->update(blackowns);  

    m_run = true;                    
    m_score = score / (boardsize * boardsize);       
}

bool Playout::passthrough(int color, int vertex) {
    assert(m_run);
    
    if (vertex == FastBoard::PASS) {
        return false;
    }
    
    return m_sq[color][vertex];
}

void Playout::do_playout_benchmark(GameState& game) {   
    float ftmp;
    int loop;    
    float len;
    float score;
    const int boardsize = game.board.get_boardsize();
    const int resign = (boardsize * boardsize) / 3;
    const int playoutlen = (boardsize * boardsize) * 2;    
    
    len = 0.0;
    score = 0;
    Time start;
    
    for (loop = 0; loop < AUTOGAMES; loop++) {
        do {                                    
            int move = game.play_random_move();                                                       
            
        } while (game.get_passes() < 2 
                 && game.get_movenum() < playoutlen
                 && abs(game.estimate_mc_score()) < resign); 
                
        len += game.get_movenum();
        ftmp = game.calculate_mc_score();   
        score += ftmp;                
                
        game.reset_game();
    }
    
    Time end;
    
    myprintf("%d games in %5.2f seconds -> %d g/s\n", 
            AUTOGAMES, 
            (float)Time::timediff(start,end)/100.0, 
            (int)((float)AUTOGAMES/((float)Time::timediff(start,end)/100.0)));
    myprintf("Avg Len: %5.2f Score: %f\n", len/(float)AUTOGAMES, score/AUTOGAMES);
}

void Playout::mc_owner(FastState & state, int iterations) {                
    const int boardsize = state.board.get_boardsize();    
    const int playoutlen = (boardsize * boardsize) * 2;                 
    
    for (int i = 0; i < iterations; i++) {
        FastState tmp = state;
        
        Playout p;
        
        p.run(tmp, false);                
    }                
}
