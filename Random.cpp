#include <time.h>
#include "config.h"

#include "Random.h"

Random* Random::s_rng = 0;

Random* Random::get_Rng(void) {
    if (s_rng == 0) {
        s_rng = new Random;
    }
    
    return s_rng;
}

Random::Random(int seed) {
    if (seed == -1) {
        seedrandom((uint32)time(0));
    } else {
        seedrandom(seed);
    }        
}

uint32 Random::randint(const uint16 max) {
    return ((random() >> 16) * max) >> 16;
}

//----------------------------------------------------------------------------//
// Maximally equidistributed Tausworthy taus88 generator by Pierre L'Ecuyer.
// See: Pierre L'Ecuyer, "Maximally Equidistributed Combined Tausworthy
//      Generators", Mathematics of Computation, 65 (1996), pp. 203-213.
//----------------------------------------------------------------------------//

uint32 Random::random(void) {      
    const uint32 mask = 0xffffffff;
    uint32 b;
    b  = (((s1 << 13) & mask) ^ s1) >> 19;
    s1 = (((s1 & 0xFFFFFFFEU) << 12) & mask) ^ b;
    b  = (((s2 << 2) & mask) ^ s2) >> 25;
    s2 = (((s2 & 0xFFFFFFF8U) << 4) & mask) ^ b;
    b  = (((s3 << 3) & mask) ^ s3) >> 11;
    s3 = (((s3 & 0xFFFFFFF0U) << 17) & mask) ^ b;
    return (s1 ^ s2 ^ s3);        
}

void Random::seedrandom(uint32 s) {
    if (s == 0) {
        s = 1;
    }        
    
    s1 = (741103597 * s);
    s2 = (741103597 * s1);
    s3 = (741103597 * s2);
    
    s1 |= 2;
    s2 |= 8;
    s3 |= 16;
} 
