#ifndef PLAYOUT_H_INCLUDED
#define PLAYOUT_H_INCLUDED

#include "GameState.h"

class Playout {
public:
    static const int AUTOGAMES = 1000000;
    static void do_playout_benchmark(GameState& game);    

    Playout(FastState &state);
    void run();
    float get_score();

private:
    FastState m_state;    
    bool m_run;
    int m_length;
    float m_score;    
};

#endif