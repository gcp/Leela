#ifndef PREPROCESS_H_INCLUDED
#define PREPROCESS_H_INCLUDED

#include <vector>

#include "FastState.h"

class Preprocess {            
public:
    Preprocess(const FastState * state);        
    
    int get_mc_own(int vtx, int tomove);
    
    typedef enum { 
        TOMOVE = 0, NEUTRAL = 1, OPPONENT = 2, INVALID = 3
    } owner_t; 
    
    owner_t get_influence(int vtx, int tomove);    
    owner_t get_moyo(int vtx, int tomove);
    
private:      
    static const int MC_OWN_ITER = 64;
    
    int m_color;
    std::vector<int> m_mc_owner;
    std::vector<int> m_influence;    
    std::vector<int> m_moyo;    
};

#endif
