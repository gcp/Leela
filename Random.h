#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include "config.h"

/*
    Random number generator xoroshiro128+
*/
class Random {
public:
    Random(int seed = -1);

    uint64 random(void);
    void seedrandom(uint32 s);

    /*
        random numbers from 0 to max
    */
    template<int MAX>
    uint32 randfix() {
        return ((random() >> 48) * MAX) >> 16;
    }

    uint32 randint16(const uint16 max);
    uint32 randint32(const uint32 max);

    /*
        random float from 0 to 1
    */
    float randflt(void);

    /*
        return the thread local RNG
    */
    static Random* get_Rng(void);

private:
    uint64 s[2];
};

#endif
