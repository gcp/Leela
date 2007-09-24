#include <vector>
#include <boost/tr1/array.hpp>
#include "config.h"

#include "Timing.h"
#include "HistoryTable.h"
#include "GameState.h"
#include "Playout.h"
#include "Utils.h"

using namespace Utils;

Playout::Playout() {    
    m_run = false;
    m_sq[0].reset();
    m_sq[1].reset();
}

float Playout::get_score() {
    assert(m_run);

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

    m_run = true;                    
    m_score = state.calculate_mc_score();                   
    
    // collect history table stats
    int wincolor;
    
    if (m_score > 0) {
        wincolor = FastBoard::BLACK;        
    } else {
        wincolor = FastBoard::WHITE;        
    }
                     
    for (int i = 0; i < FastBoard::MAXSQ; i++) {
        // playout results 
        if (m_sq[wincolor][i]) {
            HistoryTable::get_HT()->update(i, true);
        } else if (m_sq[!wincolor][i]) {
            HistoryTable::get_HT()->update(i, false);
        }
    }             
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
