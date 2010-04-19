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
        update_blackowns corresponding entry
    */            
    void update_owns(Playout::bitboard_t & blacksq, bool blackwon);        
    
    float get_blackown(const int color, const int vertex);    
    int get_blackown_i(const int color, const int vertex);
    int get_criticality_i(const int vertex);
    bool is_primed();    
    
private:   
    MCOwnerTable();

    std::vector<int> m_mcblackowner;
    std::vector<int> m_mcwinowner;
    int m_mcsimuls;
    int m_blackwins;
    SMP::Mutex m_mutex;

    static MCOwnerTable* s_mcowntable;       
};

#endif
