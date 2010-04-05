#ifndef GENETIC_H_INCLUDED
#define GENETIC_H_INCLUDED

#include "config.h"
#include <string>
#include <vector>

#include "FastState.h"

class Genetic {
public:
    void genetic_tune();
    void genetic_split(std::string filename);
    float run_simulations(FastState & state, float res);
    float run_testsets();
    void load_testsets();
    
    static std::vector<float> g_par;
    
private:    
    std::vector<FastState> m_winpool;
    std::vector<FastState> m_drawpool;
    std::vector<FastState> m_losepool;
};

#endif
