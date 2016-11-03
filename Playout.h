#ifndef PLAYOUT_H_INCLUDED
#define PLAYOUT_H_INCLUDED

#include <bitset>
#include <vector>

#include "GameState.h"
#include "FastState.h"

class Playout {
public:       
    typedef std::bitset<FastBoard::MAXSQ> bitboard_t;
    typedef std::array<bitboard_t, 2> color_bitboard_t;
    
    static const int AUTOGAMES = 100000;    
    static void do_playout_benchmark(GameState & game);
    static float mc_owner(FastState & state,
                          int iterations = 64,
                          float* points = nullptr);

    Playout();
    void run(FastState & state, bool postpassout, bool resigning);
    float get_score();
    float get_territory();
    bool passthrough(int color, int vertex);
private:
    bool m_run;
    float m_score;
    float m_territory;
    color_bitboard_t m_sq;
};

#endif
