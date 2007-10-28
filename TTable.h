#ifndef TTABLE_H_INCLUDED
#define TTABLE_H_INCLUDED

#include <vector>
#include <boost/thread.hpp>

#include "UCTNode.h"

class TTEntry {
public:
    TTEntry();
    
    uint64 m_hash;

    // XXX: need RAVE data here? 
    float m_blackwins;
    int m_visits;
};

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
    TTable(int size = 100000);

    std::vector<TTEntry> m_buckets;
    boost::mutex m_mutex;

    static TTable* s_ttable;   
};

#endif
