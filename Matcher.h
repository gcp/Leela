#ifndef MATCHER_H_INCLUDED
#define MATCHER_H_INCLUDED

#include <vector>
#include <boost/tr1/array.hpp>
#include <bitset>

class Matcher {
public:    
    Matcher();
    Matcher(std::bitset<1<<16> & pats);

    // fixed point math
    static const int UNITY = 52;
    static const int THRESHOLD = 8;

    unsigned char matches(int pattern);

    /*
        return the "global" matcher
    */            
    static Matcher* get_Matcher(void);
    static void set_Matcher(Matcher * m);
    
private:            
    static Matcher* s_matcher;   

    std::vector<unsigned char> m_patterns;
};

#endif
