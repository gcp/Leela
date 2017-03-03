#ifndef PLAYOUT_H_INCLUDED
#define PLAYOUT_H_INCLUDED

#include <bitset>
#include <vector>

#include "GameState.h"
#include "FastState.h"
#include "MCPolicy.h"

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
    void run(FastState & state, bool postpassout, bool resigning,
             PolicyTrace * trace = nullptr);
    float get_score();
    float get_territory();
    void set_eval(float eval);
    float get_eval();
    bool has_eval();
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
