#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "config.h"

#include "Timing.h"
#include "GameState.h"
#include "Playout.h"
#include "Utils.h"

using namespace Utils;

Playout::Playout() {    
    m_run = false;
}

float Playout::get_score() {
    assert(m_run);

    return m_score;
}

void Playout::run(FastState & state, bool resigning) {
    const int boardsize = state.board.get_boardsize();
    const int resign = (boardsize * boardsize) / 3;
    const int playoutlen = (boardsize * boardsize) * 2;        

     do {                                    
        state.play_random_move();                                                                   
    } while (state.get_passes() < 2 
             && state.get_movenum() < playoutlen
             && (!resigning || abs(state.estimate_score()) < resign)); 

    m_run = true;                
    m_length = state.get_movenum();
    m_score = state.calculate_mc_score();       
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
                 && abs(game.estimate_score()) < resign); 
                
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
