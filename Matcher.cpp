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
    const int max = 1 << (8 * 2);

    m_patterns.resize(max);        

    for (int i = 0; i < max; i++) {
        m_patterns[i] = pats[i];        
    }
}


// initialize matcher data
Matcher::Matcher() { 
    const int max = 1 << (8 * 2);

    m_patterns.resize(max);        

    std::fill(m_patterns.begin(), 
              m_patterns.end(),
              (unsigned char)UNITY);    
    
    int loadmax = internal_weights_fast.size();      
    int pats = 0;    

    for (int i = 0; i < loadmax; i++) {                        
        int pat = internal_patterns_fast[i];
        double wt = internal_weights_fast[i];        
        unsigned char w = (unsigned char)(wt * ((double)UNITY));        
        
        w = std::max(w, (unsigned char)1);
        
        pat &= ~(1 << 16);
            
        m_patterns[pat] = w; 
        pats++;        
    }
    
    Utils::myprintf("Loaded %d fast patterns\n", pats);
}

unsigned char Matcher::matches(int pattern) {
    return m_patterns[pattern];
}