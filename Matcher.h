#ifndef MATCHER_H_INCLUDED
#define MATCHER_H_INCLUDED

#include <vector>
#include <boost/tr1/array.hpp>
#include <bitset>

class Matcher {
public:    
    Matcher();    
    
    static const int UNITY = 2;
    static const int THRESHOLD = 2;
    static const int PROXFACTOR = 40;        

    unsigned char matches(int pattern);

    /*
        return the "global" matcher
    */            
    static Matcher* get_Matcher(void);
    static void set_Matcher(Matcher * m);

private:            
    static unsigned char clip(int val);

    static Matcher* s_matcher;   

    std::vector<unsigned char> m_patterns;
};

#endif
