#include <stdlib.h>
#include <stdio.h>
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
}

float Playout::get_score() {
    assert(m_run);

    return m_score;
}

void Playout::run(FastState & state, bool resigning) {
    const int boardsize = state.board.get_boardsize();
    const int resign = (boardsize * boardsize) / 3;
    const int playoutlen = (boardsize * boardsize) * 2;    
    
    std::tr1::array<bool, FastBoard::MAXSQ> whitevtx;
    std::tr1::array<bool, FastBoard::MAXSQ> blackvtx;    
    
    std::fill(whitevtx.begin(), whitevtx.end(), false);
    std::fill(blackvtx.begin(), blackvtx.end(), false);

     do {                                    
        int move = state.play_random_move();  
        
        if (move == FastBoard::PASS) {
            move = 0;
        }
        
        // state after move
        if (state.get_to_move() == FastBoard::BLACK) {
            whitevtx[move] = true;            
        } else {
            blackvtx[move] = true;
        }                                                                 
    } while (state.get_passes() < 2 
             && state.get_movenum() < playoutlen
             && (!resigning || abs(state.estimate_score()) < resign)); 

    m_run = true;                
    m_length = state.get_movenum();
    m_score = state.calculate_mc_score();   
        
    for (int i = 0; i < whitevtx.size(); i++) {
        if (whitevtx[i]) {
            if (m_score > 0.0f) {
                HistoryTable::get_HT()->update(i, false);
            } else {
                HistoryTable::get_HT()->update(i, true);
            }
        }   
        if (blackvtx[i]) {
           if (m_score > 0.0f) {
                HistoryTable::get_HT()->update(i, true);
            } else {
                HistoryTable::get_HT()->update(i, false);
            }
        }      
    }    
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
