#include <vector>
#include <boost/tr1/array.hpp>
#include <cstdlib>
#include <cassert>
#include "config.h"

#include "Timing.h"
#include "GameState.h"
#include "Playout.h"
#include "Utils.h"
#include "MCOTable.h"
#include "Random.h"

using namespace Utils;

Playout::Playout() :
    m_run(false), m_eval_valid(false) {
    m_sq[0].reset();
    m_sq[1].reset();
}

float Playout::get_score() {
    assert(m_run);
    assert(m_score > -2.00f && m_score < 2.00f);

    return m_score;
}

float Playout::get_territory() {
    assert(m_run);
    return m_territory;
}

void Playout::set_eval(int tomove, float eval) {
    if (tomove == FastBoard::WHITE) {
        eval = 1.0f - eval;
    }
    m_black_eval = eval;
    m_eval_valid = true;
}

float Playout::get_eval(int tomove) {
    assert(m_eval_valid == true);
    if (tomove == FastBoard::BLACK) {
        return m_black_eval;
    } else {
        return 1.0f - m_black_eval;
    }
}

bool Playout::has_eval() {
    return m_eval_valid;
}

void Playout::run(FastState & state, bool resigning) {
    assert(!m_run);

    const int boardsize = state.board.get_boardsize();

    const int resign = (boardsize * boardsize) / 3;
    const int playoutlen = (boardsize * boardsize) * 2;

    int counter = 0;

    // do the main loop
    while (state.get_passes() < 2
        && state.get_movenum() < playoutlen
        && (!resigning || abs(state.estimate_mc_score()) < resign)) {
        int vtx = state.play_random_move();

        if (counter < 30 && vtx != FastBoard::PASS) {
            int color = !state.get_to_move();

            if (!m_sq[!color][vtx]) {
                m_sq[color][vtx] = true;
            }
        }

        counter++;
    }

    // get ownership info
    bitboard_t blackowns;

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
        }
    }

    float score = state.calculate_mc_score();

    // update MCO in one swoop
    bool blackwon;
    if (score == 0.0f) {
        blackwon = (Random::get_Rng()->randint(2) == 0);
    } else {
        blackwon = (score > 0.0f);
    }
    MCOwnerTable::get_MCO()->update_owns(blackowns, blackwon);

    m_run = true;
    m_territory = score;
    // Scale to -1.0 <--> 1.0
    m_score = score / (boardsize * boardsize);
}

bool Playout::passthrough(int color, int vertex) {
    assert(m_run);
    
    if (vertex == FastBoard::PASS) {
        return false;
    }
    
    return m_sq[color][vertex];
}

void Playout::do_playout_benchmark(GameState & game) {   
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
            game.play_random_move();
            
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

float Playout::mc_owner(FastState & state, int iterations, float* points) {
    float bwins = 0.0f;
    float board_score = 0.0f;

    for (int i = 0; i < iterations; i++) {
        FastState tmp = state;

        Playout p;

        p.run(tmp, false);

        float score = p.get_score();
        if (score == 0.0f) {
            bwins += 0.5f;
        } else if (score > 0.0f) {
            bwins += 1.0f;
        }

        board_score += p.get_territory();
    }

    float score = bwins / (float)iterations;
    float territory = board_score / (float)iterations;

    if (state.get_to_move() != FastBoard::BLACK) {
        score = 1.0f - score;
    }

    if (points != nullptr) {
        *points = territory;
    }

    return score;
}
