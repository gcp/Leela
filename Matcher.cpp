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

Matcher::Matcher(std::bitset<1<<16> & pats) {
    //const int max = 1 << (8 * 2);

    //m_patterns.resize(max);        

    //for (int i = 0; i < max; i++) {
    //    m_patterns[i] = pats[i];        
    //}
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
                   
        m_patterns[FastBoard::BLACK][i] = internal_weights_fast[reducpat1];
        m_patterns[FastBoard::WHITE][i] = internal_weights_fast[reducpat2];
    }           
}

unsigned char Matcher::matches(int color, int pattern) {
    return m_patterns[color][pattern];
}