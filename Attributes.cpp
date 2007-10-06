#include <algorithm>

#include "Attributes.h"
#include "FastBoard.h"

Attributes::Attributes() {    
    m_present.reserve(32);
}

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
    
    mindist = std::min(x, bsize - x);
    mindist = std::min(mindist, y);
    mindist = std::min(mindist, bsize - y);

    return mindist; 
}

void Attributes::get_from_move(KoState * state, int vtx) {
    m_present.clear();

    int tomove = state->get_to_move();

    // saving size
    // 0, 1, 2, 3, >3
    int ss;
    if (vtx != FastBoard::PASS) {
        ss = state->board.saving_size(tomove, vtx);
    } else {
        ss = -1;
    }
    m_present.push_back(ss == 0);
    m_present.push_back(ss == 1);
    m_present.push_back(ss == 2);
    m_present.push_back(ss == 3);
    m_present.push_back(ss  > 3);    

    // capture size
    // 0, 1, 2, 3, >3
    int cs;
    if (vtx != FastBoard::PASS) {
        cs = state->board.capture_size(tomove, vtx);
    } else {
        cs = -1;
    }
    m_present.push_back(cs == 0);
    m_present.push_back(cs == 1);
    m_present.push_back(cs == 2);
    m_present.push_back(cs == 3);
    m_present.push_back(cs  > 3);

    // self-atari
    bool sa;
    if (vtx != FastBoard::PASS) {
        sa = state->board.self_atari(tomove, vtx);
    } else {
        sa = false;
    }
    m_present.push_back(sa);

    // eyefill
    bool ef;
    if (vtx != FastBoard::PASS) {
        ef = !state->board.no_eye_fill(vtx);
    } else {
        ef = false;
    }
    m_present.push_back(ef);

    // atari size
    // semi-atari liberty count

    // pass
    bool ps = (vtx == FastBoard::PASS) && (state->get_last_move() != FastBoard::PASS);
    bool pp = (vtx == FastBoard::PASS) && (state->get_last_move() == FastBoard::PASS);
    m_present.push_back(ps);
    m_present.push_back(pp);

    // border    
    int borddist;
    if (vtx != FastBoard::PASS) {
        borddist = border_distance(state->board.get_xy(vtx), 
                                   state->board.get_boardsize());
    } else {
        borddist = 100;
    }
    m_present.push_back(borddist == 0);
    m_present.push_back(borddist == 1);
    m_present.push_back(borddist == 2);
    m_present.push_back(borddist == 3);
    m_present.push_back(borddist == 4);
    m_present.push_back(borddist == 5);    

    // prev move distance
    int prevdist;
    if (state->get_last_move() != FastBoard::PASS && vtx != FastBoard::PASS) {
        prevdist = move_distance(state->board.get_xy(state->get_last_move()), 
                                 state->board.get_xy(vtx));
    } else {
        prevdist = 100;
    }
    m_present.push_back(prevdist == 2);
    m_present.push_back(prevdist == 3);
    m_present.push_back(prevdist == 4);
    m_present.push_back(prevdist == 5);
    m_present.push_back(prevdist == 6);
    m_present.push_back(prevdist == 7);
    m_present.push_back(prevdist == 8);
    m_present.push_back(prevdist == 9);
    m_present.push_back(prevdist >  9);

    // prev prev move

    // mc owner
    
    // shape 
    int pat;
    if (vtx != FastBoard::PASS) {
        pat = state->board.get_pattern(vtx);   
    } else {
        pat = 0;
    }
    if (!state->board.black_to_move()) {
        // this is similar as adding vtx in the middle
        pat |= (1 << 15);
    }
    m_pattern = pat;
}
