#include <time.h>
#include <limits.h>
#include <thread>
#include "config.h"

#include "Random.h"

Random* Random::get_Rng(void) {
    static thread_local Random s_rng;
    return &s_rng;
}

Random::Random(int seed) {
    if (seed == -1) {
        size_t thread_id =
            std::hash<std::thread::id>()(std::this_thread::get_id());
        seedrandom((uint32)time(0) ^ (uint32)thread_id);
    } else {
        seedrandom(seed);
    }
}

static inline uint64 rotl(const uint64 x, int k) {
	return (x << k) | (x >> (64 - k));
}

uint64 Random::random(void) {
    const uint64 s0 = s[0];
    uint64 s1 = s[1];
    const uint64 result = s0 + s1;

    s1 ^= s0;
    s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);
    s[1] = rotl(s1, 36);

    return result;
}

uint32 Random::randint16(const uint16 max) {
    return ((random() >> 48) * max) >> 16;
}

uint32 Random::randint32(const uint32 max) {
    return ((random() >> 32) * (uint64)max) >> 32;
}

void Random::seedrandom(uint32 seed) {
    s[0] = (741103597 * seed);
    s[1] = (741103597 * s[0]);
}

float Random::randflt(void) {
    // We need a 23 bit mantissa + implicit 1 bit = 24 bit number
    // starting from a 64 bit random.
    constexpr float umax = 1.0f / (UINT32_C(1) << 24);
    uint32 rnd = random() >> 40;
    return ((float)rnd) * umax;
}
