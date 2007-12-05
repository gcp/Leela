#ifndef MATCHER_H_INCLUDED
#define MATCHER_H_INCLUDED

#include <vector>
#include <boost/tr1/array.hpp>

class Matcher {
public:    
    Matcher();

    // fixed point math
    static const int UNITY = 5;

    unsigned char matches(int color, int pattern);

    /*
        return the "global" matcher
    */            
    static Matcher* get_Matcher(void);
    
private:            
    static Matcher* s_matcher;   

    std::tr1::array<std::vector<unsigned char>, 2> m_patterns;
};

#endif
