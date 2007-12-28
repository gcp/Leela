#include <algorithm>

#include "Attributes.h"
#include "FastBoard.h"
#include "MCOTable.h"

int Attributes::move_distance(std::pair<int, int> xy1, 
                              std::pair<int, int> xy2) {
    int dx = abs(xy1.first  - xy2.first);
    int dy = abs(xy1.second - xy2.second);

    return dx + dy + std::max(dx, dy);
}

int Attributes::border_distance(std::pair<int, int> xy, int bsize) {
    int mindist;
    int x = xy.first;
    int y = xy.second;
    
    mindist = std::min(x, bsize - x - 1);
    mindist = std::min(mindist, y);
    mindist = std::min(mindist, bsize - y - 1);

    return mindist; 
}

void FastAttributes::get_from_move(FastState * state, int vtx) {
    m_present.reset();

    int tomove = state->get_to_move();
    int bitpos = 0;                                                           
    
    // prev move distance
    int prevdist;
    if (state->get_last_move() > 0 && vtx > 0) {
        prevdist = Attributes::move_distance(state->board.get_xy(state->get_last_move()), 
                                             state->board.get_xy(vtx));
    } else {
        prevdist = 100;
    }
    
    m_present[bitpos++] = (prevdist <=  3);        
          
    // atari-escape (saving-size) adding liberties (only count pseudos)
    // adding 1 is self-atari so doesn't count    
    int ae;
    int ss;
    if (vtx != FastBoard::PASS) {
        ss = state->board.saving_size(tomove, vtx);
        ae = state->board.count_liberties(vtx);
    } else {
        ss = -1;
        ae = -1;
    }            
    m_present[bitpos++] = (ss > 0);    
    
    // capture size    
    int cs;
    if (vtx != FastBoard::PASS) {
        cs = state->board.capture_size(tomove, vtx);
    } else {
        cs = -1;
    }    
    m_present[bitpos++] = (cs > 0);
    
    // sa
    bool sa;
    if (vtx != FastBoard::PASS) {
        sa = state->board.self_atari(tomove, vtx);
    } else {
        sa = false;
    }    
    m_present[bitpos++] = sa;
    
    // shape  (border check)            
    int pat;
    if (vtx != FastBoard::PASS) {                              
        pat = state->board.get_pattern3(vtx, tomove);                        
    } else {
        pat = 0xFFFF; // all INVAL
    }       

    m_pattern = pat;
}

void Attributes::get_from_move(FastState * state,  
                               std::vector<int> & territory,
                               std::vector<int> & moyo,
                               int vtx) {
    m_present.reset();

    int tomove = state->get_to_move();
    int bitpos = 0;

    // saving size
    // 0, 1, 2, 3, >3
    int ss;
    if (vtx != FastBoard::PASS) {
        ss = state->board.saving_size(tomove, vtx);
    } else {
        ss = -1;
    }        
    m_present[bitpos++] = (ss == 1);
    m_present[bitpos++] = (ss == 2);
    m_present[bitpos++] = (ss == 3);
    m_present[bitpos++] = (ss == 4);
    m_present[bitpos++] = (ss  > 4);    
    
    // atari-escape (saving-size) adding liberties (only count pseudos)
    // adding 1 is self-atari so doesn't count    
    int ae;
    if (vtx != FastBoard::PASS) {
        ae = state->board.count_liberties(vtx);
    } else {
        ae = -1;
    }        
    m_present[bitpos++] = (ss > 0 && ae == 2);
    m_present[bitpos++] = (ss > 0 && ae == 3);
    
    // liberty increase
    int al;
    if (vtx != FastBoard::PASS) {
        al = state->board.minimum_elib_count(!tomove, vtx);
        // isolated stone
        if (al == 100) {
            al = 0;
        }
    } else {
        al = -1;
    }        
    
    m_present[bitpos++] = (al ==  2 && ae == 2);
    m_present[bitpos++] = (al ==  3 && ae == 2);
    m_present[bitpos++] = (al ==  4 && ae == 2);
    m_present[bitpos++] = (al ==  5 && ae == 2);
    m_present[bitpos++] = (al ==  6 && ae == 2);
    m_present[bitpos++] = (al >   6 && ae == 2);    
    m_present[bitpos++] = (al ==  2 && ae == 3);
    m_present[bitpos++] = (al ==  3 && ae == 3);
    m_present[bitpos++] = (al ==  4 && ae == 3);
    m_present[bitpos++] = (al ==  5 && ae == 3);
    m_present[bitpos++] = (al ==  6 && ae == 3);
    m_present[bitpos++] = (al >   6 && ae == 3);

    // capture size
    // 0, 1, 2, 3, >3
    int cs;
    if (vtx != FastBoard::PASS) {
        cs = state->board.capture_size(tomove, vtx);
    } else {
        cs = -1;
    }    
    m_present[bitpos++] = (cs == 1);
    m_present[bitpos++] = (cs == 2);
    m_present[bitpos++] = (cs == 3);
    m_present[bitpos++] = (cs == 4);
    m_present[bitpos++] = (cs  > 4);    

    // self-atari
    bool sa;
    if (vtx != FastBoard::PASS) {
        sa = state->board.self_atari(tomove, vtx);
    } else {
        sa = false;
    }
    m_present[bitpos++] = (sa);
    
    // generalized atari
    int at;
    if (vtx != FastBoard::PASS) {
        at = state->board.minimum_elib_count(tomove, vtx);
    } else {
        at = -1;
    }        
    
    m_present[bitpos++] = (at == 2) && (state->komove != -1);  // atari with ko
    m_present[bitpos++] = (at == 3) && (state->komove != -1);  // semiatari with ko
    m_present[bitpos++] = (at == 2);                           // atari
    m_present[bitpos++] = (at == 3);
    m_present[bitpos++] = (at == 4);
    m_present[bitpos++] = (at == 5);
    m_present[bitpos++] = (at == 6);
    m_present[bitpos++] = (at >  6);               
                     
    // pass
    bool ps   = (vtx == FastBoard::PASS) && (state->get_passes() == 0);
    bool psps = (vtx == FastBoard::PASS) && (state->get_passes() == 1);
    m_present[bitpos++] = (ps);
    m_present[bitpos++] = (psps);

    // border    
    int borddist;
    if (vtx != FastBoard::PASS) {
        borddist = border_distance(state->board.get_xy(vtx), 
                                   state->board.get_boardsize());
    } else {
        borddist = -1;
    }
    m_present[bitpos++] = (borddist == 0);
    m_present[bitpos++] = (borddist == 1);
    m_present[bitpos++] = (borddist == 2);
    m_present[bitpos++] = (borddist == 3);
    m_present[bitpos++] = (borddist == 4);
    m_present[bitpos++] = (borddist >  4);                 
        
    // mcowner
    float mcown;
    if (vtx != FastBoard::PASS) {
        mcown = MCOwnerTable::get_MCO()->get_score(tomove, vtx);
    } else {
        mcown = -1.0f;
    }
    m_present[bitpos++] = (mcown >= -0.01f && mcown <  0.10);
    m_present[bitpos++] = (mcown >=  0.10f && mcown <  0.20);
    m_present[bitpos++] = (mcown >=  0.20f && mcown <  0.30);
    m_present[bitpos++] = (mcown >=  0.30f && mcown <  0.40);
    m_present[bitpos++] = (mcown >=  0.40f && mcown <  0.50);
    m_present[bitpos++] = (mcown >=  0.50f && mcown <  0.60);
    m_present[bitpos++] = (mcown >=  0.60f && mcown <  0.70);
    m_present[bitpos++] = (mcown >=  0.70f && mcown <  0.80);
    m_present[bitpos++] = (mcown >=  0.80f && mcown <  0.90);
    m_present[bitpos++] = (mcown >=  0.90f && mcown <  1.01);   
    
    int point = vtx;        
    if (vtx == FastBoard::PASS) {
        point = 0;
    }
    m_present[bitpos++] = (territory[point] != 0);
    m_present[bitpos++] = (moyo[point] != 0);
    
    // prev move distance
    int prevdist;
    if (state->get_last_move() > 0 && vtx > 0) {
        prevdist = move_distance(state->board.get_xy(state->get_last_move()), 
                                 state->board.get_xy(vtx));
    } else {
        prevdist = -1;
    }
    m_present[bitpos++] = (prevdist ==  2);
    m_present[bitpos++] = (prevdist ==  3);
    m_present[bitpos++] = (prevdist ==  4);
    m_present[bitpos++] = (prevdist ==  5);
    m_present[bitpos++] = (prevdist ==  6);
    m_present[bitpos++] = (prevdist ==  7);
    m_present[bitpos++] = (prevdist ==  8);
    m_present[bitpos++] = (prevdist ==  9);
    m_present[bitpos++] = (prevdist == 10);
    m_present[bitpos++] = (prevdist == 11);
    m_present[bitpos++] = (prevdist == 12);
    m_present[bitpos++] = (prevdist == 13);
    m_present[bitpos++] = (prevdist == 14);
    m_present[bitpos++] = (prevdist == 15);    
    m_present[bitpos++] = (prevdist == 16);  
    m_present[bitpos++] = (prevdist  > 16);    
    
    // prev prev move
    int prevprevdist;
    if (state->get_prevlast_move() > 0 && vtx > 0) {
        prevprevdist = move_distance(state->board.get_xy(vtx), 
                                     state->board.get_xy(state->get_prevlast_move()));
    } else {
        prevprevdist = -1;
    }
    
    m_present[bitpos++] = (prevprevdist ==  2);
    m_present[bitpos++] = (prevprevdist ==  3);
    m_present[bitpos++] = (prevprevdist ==  4);
    m_present[bitpos++] = (prevprevdist ==  5);
    m_present[bitpos++] = (prevprevdist ==  6);    
    m_present[bitpos++] = (prevprevdist ==  7);
    m_present[bitpos++] = (prevprevdist ==  8);
    m_present[bitpos++] = (prevprevdist ==  9);
    m_present[bitpos++] = (prevprevdist == 10);

    m_present[bitpos++] = (prevprevdist == 11);
    m_present[bitpos++] = (prevprevdist == 12);
    m_present[bitpos++] = (prevprevdist == 13);
    m_present[bitpos++] = (prevprevdist == 14);
    m_present[bitpos++] = (prevprevdist == 15);
    m_present[bitpos++] = (prevprevdist == 16);
    m_present[bitpos++] = (prevprevdist >  16);      
    
    // shape  (border check)            
    int pat;
    if (vtx != FastBoard::PASS) {          
        if (borddist < 1) {              
            pat = state->board.get_pattern4(vtx, !state->board.black_to_move(), true);        
        } else {
            pat = state->board.get_pattern4(vtx, !state->board.black_to_move(), false);
        }                        
    } else {
        pat = 0xFFFFFF; // all INVAL
    }       

    m_pattern = pat;
}


uint64 Attributes::get_pattern(void) {
    return m_pattern;
}

uint64 FastAttributes::get_pattern(void) {
    return m_pattern;
}

bool Attributes::attribute_enabled(int idx) {
    return m_present[idx];
}

bool FastAttributes::attribute_enabled(int idx) {
    return m_present[idx];
}
