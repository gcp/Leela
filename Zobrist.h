#ifndef ZOBRIST_H_INCLUDED
#define ZOBRIST_H_INCLUDED

#include <boost/tr1/array.hpp>

#include "FastBoard.h"
#include "Random.h"

class Zobrist {
public:
    static std::tr1::array<std::tr1::array<uint64, FastBoard::MAXSQ>,     4> zobrist;
    static std::tr1::array<std::tr1::array<uint64, FastBoard::MAXSQ * 2>, 2> zobrist_pris;
    static std::tr1::array<uint64, 4>                                        zobrist_pass;

    static void init_zobrist(Random & rng);   
};

#endif