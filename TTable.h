#ifndef TTABLE_H_INCLUDED
#define TTABLE_H_INCLUDED

#include <vector>

#include "TTEntry.h"

class TTable {
public:    
    /*
        return the global TT
    */            
    static TTable* get_TT(void);
    
private:   
    TTable(int size = 10000);

    std::vector<TTEntry> m_buckets;

    static TTable* s_ttable;   
};


#endif