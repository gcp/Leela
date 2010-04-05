#ifndef MCOTABLE_H_INCLUDED
#define MCOTABLE_H_INCLUDED

#include <vector>
#include "Playout.h"
#include "SMP.h"

class MCOwnerTable {
public:        
    /*
        return the global TT
    */            
    static MCOwnerTable * get_MCO();
    static void clear();    
    
    /*
        update corresponding entry
    */            
    void update(Playout::bitboard_t & blacksq);    
    
    float get_score(const int color, const int vertex);    
    bool is_primed();    
    
private:   
    MCOwnerTable();

    std::vector<int> m_mcowner;
    int m_mcsimuls;
    SMP::Mutex m_mutex;

    static MCOwnerTable* s_mcowntable;       
};

#endif
