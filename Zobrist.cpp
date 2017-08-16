#include "config.h"

#include "Random.h"
#include "Zobrist.h"

std::array<std::array<uint64, FastBoard::MAXSQ>,     4> Zobrist::zobrist;
std::array<std::array<uint64, FastBoard::MAXSQ * 2>, 2> Zobrist::zobrist_pris;
std::array<uint64, 5>                                   Zobrist::zobrist_pass;

void Zobrist::init_zobrist(Random & rng) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < FastBoard::MAXSQ; j++) {
            Zobrist::zobrist[i][j]  = ((uint64)rng.randuint32()) << 32;
            Zobrist::zobrist[i][j] ^= (uint64)rng.randuint32();
        }
    }

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < FastBoard::MAXSQ * 2; j++) {
            Zobrist::zobrist_pris[i][j]  = ((uint64)rng.randuint32()) << 32;
            Zobrist::zobrist_pris[i][j] ^= (uint64)rng.randuint32();
        }
    }

    for (int i = 0; i < 5; i++) {
        Zobrist::zobrist_pass[i]  = ((uint64)rng.randuint32()) << 32;
        Zobrist::zobrist_pass[i] ^= (uint64)rng.randuint32();
    }
}
