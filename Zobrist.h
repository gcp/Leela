#ifndef ZOBRIST_H_INCLUDED
#define ZOBRIST_H_INCLUDED

#include <array>

#include "FastBoard.h"
#include "Random.h"

class Zobrist {
public:
    static std::array<std::array<uint64, FastBoard::MAXSQ>,     4> zobrist;
    static std::array<std::array<uint64, FastBoard::MAXSQ * 2>, 2> zobrist_pris;
    static std::array<uint64, 5>                                   zobrist_pass;

    static void init_zobrist(Random & rng);
};

#endif
