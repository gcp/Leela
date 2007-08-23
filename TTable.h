#ifndef TTABLE_H_INCLUDED
#define TTABLE_H_INCLUDED

#include <vector>

#include "TTEntry.h"
#include "UCTNode.h"

class TTable {
public:    
    /*
        return the global TT
    */            
    static TTable* get_TT(void);
    
    /*
        update corresponding entry
    */            
    void update(uint64 hash, const UCTNode * node); 
    
    /*
        sync given node with TT
    */        
    void sync(uint64 hash, UCTNode * node);
    
private:   
    TTable(int size = 10000);

    std::vector<TTEntry> m_buckets;

    static TTable* s_ttable;   
};


#endif