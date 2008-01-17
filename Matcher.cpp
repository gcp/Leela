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

// initialize matcher data
Matcher::Matcher() { 
    const int max = 65536;    

    m_patterns.resize(max);                     
    std::fill(m_patterns.begin(), m_patterns.end(), 
              clip(Matcher::UNITY * Matcher::PROXFACTOR));        

    typedef std::map<int, float> patmap;    
    patmap patweights;
    
    for (int i = 0; i < internal_weights_fast.size(); i++) {
        patweights.insert(std::make_pair(internal_patterns_fast[i],
                                         internal_weights_fast[i]));
    }    
    
    for (int i = 0; i < max; i++) {                               
        patmap::iterator it = patweights.find(i);                       
        
        if (it != patweights.end()) {
            float weight = it->second * (Matcher::PROXFACTOR * Matcher::UNITY);
            m_patterns[i] = clip((int)(weight + 0.5f));
        }     
    }           
}

unsigned char Matcher::matches(int pattern) {
    int pat = ((pattern * 1597334677) >> 16) & 0xFFFF; 
    return m_patterns[pat];
}

unsigned char Matcher::clip(int val) {
    if (val < 0) {
        val = 0;
    } else if (val > 255) {
        val = 255;
    }
    
    return val;
}
