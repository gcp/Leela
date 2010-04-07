#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include "config.h"

/*
    Random number generator
*/    
class Random {    
public:    
    Random(int seed = -1);
    
    uint32 random(void);
    void seedrandom(uint32 s);
    /*
        random number from 0 to max
    */            
    uint32 randint(const uint16 max);
    uint32 randint32(const uint32 max);

    /*
        random float from 0 to 1
    */
    float randflt(void);
    
    /*
        return the "global" RNG
    */            
    static Random* get_Rng(void);
    
private:    
    uint32 s1, s2, s3;   
    
    static Random* s_rng;        
};

#endif
