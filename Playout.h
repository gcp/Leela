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
    void set_eval(float eval);
    float get_eval() const;
    bool has_eval() const;
    bool passthrough(int color, int vertex);
private:
    bool m_run;
    float m_score;
    float m_territory;
    bool m_eval_valid;
    float m_blackeval;
    color_bitboard_t m_sq;
};

#endif
