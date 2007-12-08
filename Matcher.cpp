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

// initialize matcher data
Matcher::Matcher() { 
    const int max = 1 << (8 * 2);

    m_patterns[FastBoard::BLACK].resize(max);    
    m_patterns[FastBoard::WHITE].resize(max); 

    std::fill(m_patterns[FastBoard::BLACK].begin(), 
              m_patterns[FastBoard::BLACK].end(),
              (unsigned char)UNITY);    
    std::fill(m_patterns[FastBoard::WHITE].begin(), 
              m_patterns[FastBoard::WHITE].end(),
              (unsigned char)UNITY);            
 
    int loadmax = internal_weights_fast.size();      
    int wpats = 0;
    int bpats = 0;

    for (int i = 0; i < loadmax; i++) {                        
        int pat = internal_patterns_fast[i];
        double wt = internal_weights_fast[i];        
        unsigned char w = (unsigned char)(wt * ((double)UNITY));        
        
        w = std::max(w, (unsigned char)1);
        
        if (pat & (1 << 16)) {
            pat &= ~(1 << 16);
            m_patterns[FastBoard::WHITE][pat] = w;
            wpats++;
        } else {
            m_patterns[FastBoard::BLACK][pat] = w; 
            bpats++;
        }                        
    }
    
    Utils::myprintf("Loaded %d black and %d white fast patterns\n", wpats, bpats);
}

unsigned char Matcher::matches(int color, int pattern) {
    return m_patterns[color][pattern];
}