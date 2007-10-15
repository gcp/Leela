#include "config.h"

#include "Preprocess.h"
#include "Playout.h"
#include "Utils.h"

Preprocess::Preprocess(const FastState * state) {    
    FastState tmp = *state;
    int color = tmp.get_to_move();
    
    m_color = color;
    m_mc_owner = Playout::mc_owner(tmp, color, MC_OWN_ITER);        
    
    // remove dead groups from tmp state
    for (int i = 0; i < tmp.board.m_maxsq; i++) {
        if (m_mc_owner[i] <= 12) {
            if (tmp.board.get_square(i) == color) {
                // own stone is dead
                tmp.board.set_square(i, FastBoard::EMPTY);
            }    
        } else if (m_mc_owner[i] >= 52) {
            if (tmp.board.get_square(i) == !color) {
                // enemy stone is dead
                tmp.board.set_square(i, FastBoard::EMPTY);
            }
        }
    }
    
    m_influence = tmp.board.influence();    
    m_moyo = tmp.board.moyo();
    
    /*Utils::myprintf("\n");
    tmp.board.display_map(m_influence);  
    Utils::myprintf("\n");    
    tmp.board.display_map(m_moyo);  
    Utils::myprintf("\n");
    tmp.board.display_map(m_area);  
    Utils::myprintf("\n");*/
}

int Preprocess::get_mc_own(int vtx, int tomove) {
    int mcown = m_mc_owner[vtx];
    
    if (tomove != m_color) {
        mcown = MC_OWN_ITER - mcown - 1;
    }
    
    return mcown; 
}

Preprocess::owner_t Preprocess::get_influence(int vtx, int tomove) {
    if (tomove == FastBoard::BLACK) {
        if (m_influence[vtx] > 0) {
            return TOMOVE;
        } else if (m_influence[vtx] == 0) {
            return NEUTRAL;
        } else {
            return OPPONENT;
        }
    } else {
        if (m_influence[vtx] > 0) {
            return OPPONENT;
        } else if (m_influence[vtx] == 0) {
            return NEUTRAL;
        } else {
            return TOMOVE;
        }
    }
}

Preprocess::owner_t Preprocess::get_moyo(int vtx, int tomove) {
    if (tomove == FastBoard::BLACK) {
        if (m_moyo[vtx] > 0) {
            return TOMOVE;
        } else if (m_moyo[vtx] == 0) {
            return NEUTRAL;
        } else {
            return OPPONENT;
        }
    } else {
        if (m_moyo[vtx] > 0) {
            return OPPONENT;
        } else if (m_moyo[vtx] == 0) {
            return NEUTRAL;
        } else {
            return TOMOVE;
        }
    }
}
