#ifndef PLAYOUT_H_INCLUDED
#define PLAYOUT_H_INCLUDED

#include <bitset>
#include <vector>
#include <boost/tr1/array.hpp>

#include "GameState.h"
#include "FastState.h"

class Playout {
public:
    static const int AUTOGAMES = 100000;
    static void do_playout_benchmark(GameState & game);    

    Playout();
    void run(FastState & state, bool resigning = true);
    float get_score();    
    void set_final_score(float score);
    bool passthrough(int color, int vertex);    
    static std::vector<int> mc_owner(FastState & state, int color, 
                                     int iterations = 64);

private:        
    bool m_run;    
    float m_score; 
    
    std::tr1::array<std::bitset<FastBoard::MAXSQ>, 2> m_sq;    
};

#endif
