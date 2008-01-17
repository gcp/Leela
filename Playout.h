#ifndef PLAYOUT_H_INCLUDED
#define PLAYOUT_H_INCLUDED

#include <bitset>
#include <vector>
#include <boost/tr1/array.hpp>

#include "GameState.h"
#include "FastState.h"

class Playout {
public:       
    typedef std::bitset<FastBoard::MAXSQ> bitboard_t;
    typedef std::tr1::array<bitboard_t, 2> color_bitboard_t;
    
    static const int AUTOGAMES = 50000;    
    static void do_playout_benchmark(GameState & game);            
    static void mc_owner(FastState & state, int iterations = 32);

    Playout();
    void run(FastState & state, bool resigning = true);
    float get_score();    
    void set_final_score(float score);
    bool passthrough(int color, int vertex);         
private:                         
    bool m_run;    
    float m_score;             
    color_bitboard_t m_sq;                
};

#endif
