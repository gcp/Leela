#ifndef PLAYOUT_H_INCLUDED
#define PLAYOUT_H_INCLUDED

#include <bitset>
#include <vector>

#include "GameState.h"
#include "FastState.h"
#include "MCPolicy.h"

class Playout {
public:
    using bitboard_t = std::bitset<FastBoard::MAXSQ>;
    using color_bitboard_t = std::array<bitboard_t, 2>;

    static const int AUTOGAMES = 200000;
    static void do_playout_benchmark(GameState & game);
    static float mc_owner(FastState & state,
                          const int iterations = 64,
                          float* points = nullptr);

    Playout();
    void run(FastState & state, bool postpassout, bool resigning,
             PolicyTrace * trace = nullptr);
    float get_score() const;
    float get_territory() const;
    bool passthrough(int color, int vertex);
private:
    bool m_run;
    float m_score;
    float m_territory;
    color_bitboard_t m_sq;
};

#endif
