#include "config.h"

#include <map>

#include "Matcher.h"
#include "FastBoard.h"
#include "WeightsMatcher.h"
#include "Utils.h"

Matcher* Matcher::s_matcher = 0;

Matcher* Matcher::get_Matcher(void) {
    if (s_matcher == 0) {
        s_matcher = new Matcher;
    }
    
    return s_matcher;
}

void Matcher::set_Matcher(Matcher * m) {
    if (s_matcher) {
        delete s_matcher;
    }
    
    s_matcher = m;
}

Matcher::Matcher(std::tr1::array<unsigned char, 65536> & pats) {
    const int max = 1 << (8 * 2);

    m_patterns[FastBoard::BLACK].resize(max);    
    m_patterns[FastBoard::WHITE].resize(max);    
   
     // minimal board we need is 3x3
    FastBoard board;
    board.reset_board(3);        
     
    // center square
    int startvtx = board.get_vertex(1, 1);
    
    for (int i = 0; i < max; i++) {
        int w = i;
        // fill board
        for (int k = 0; k < 8; k++) {
            board.set_square(startvtx + board.get_extra_dir(k), 
                             static_cast<FastBoard::square_t>(w & 3));
            w = w >> 2;
        }     
        int reducpat1 = board.get_pattern3(startvtx, false);
        int reducpat2 = board.get_pattern3(startvtx, true);

        m_patterns[FastBoard::BLACK][i] = pats[reducpat1];
        m_patterns[FastBoard::WHITE][i] = pats[reducpat2];
    }           
}


// initialize matcher data
Matcher::Matcher() { 
    const int max = 1 << (8 * 2);

    m_patterns[FastBoard::BLACK].resize(max);    
    m_patterns[FastBoard::WHITE].resize(max);    
   
    // minimal board we need is 3x3
    FastBoard board;
    board.reset_board(3);        
     
    // center square
    int startvtx = board.get_vertex(1, 1);
    
    typedef std::map<int, float> patmap;    
    patmap patweights;
    
    for (int i = 0; i < internal_weights_fast.size(); i++) {
        patweights.insert(std::make_pair(internal_patterns_fast[i],
                                         internal_weights_fast[i]));
    }    
    
    for (int i = 0; i < max; i++) {
        int w = i;
        // fill board
        for (int k = 0; k < 8; k++) {
            board.set_square(startvtx + board.get_extra_dir(k), 
                             static_cast<FastBoard::square_t>(w & 3));
            w = w >> 2;
        }     
        int reducpat1 = board.get_pattern3(startvtx, false);
        int reducpat2 = board.get_pattern3(startvtx, true);
        
        patmap::iterator it = patweights.find(reducpat1);
        
        if (it != patweights.end()) {
            float weight = it->second * (Matcher::PROXFACTOR * Matcher::UNITY);
            m_patterns[FastBoard::BLACK][i] = clip((int)(weight + 0.5f));
        } else {
            m_patterns[FastBoard::BLACK][i] = clip(Matcher::UNITY * Matcher::PROXFACTOR);
        }
        
        it = patweights.find(reducpat2);
        
        if (it != patweights.end()) {
            float weight = it->second * (Matcher::PROXFACTOR * Matcher::UNITY);
            m_patterns[FastBoard::WHITE][i] = clip((int)(weight + 0.5f));
        } else {
            m_patterns[FastBoard::WHITE][i] = clip(Matcher::UNITY * Matcher::PROXFACTOR);
        }
                           
        //m_patterns[FastBoard::BLACK][i] = board.match_pattern(FastBoard::BLACK, startvtx);
        //m_patterns[FastBoard::WHITE][i] = board.match_pattern(FastBoard::WHITE, startvtx);        
    }           
}

unsigned char Matcher::matches(int color, int pattern) {
    return m_patterns[color][pattern];
}

unsigned char Matcher::clip(int val) {
    if (val < 0) {
        val = 0;
    } else if (val > 255) {
        val = 255;
    }
    
    return val;
}
