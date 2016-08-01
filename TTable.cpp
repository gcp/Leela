#include "config.h"

#include <vector>

#include "Utils.h"
#include "TTable.h"

TTable* TTable::s_ttable = 0;

TTable* TTable::get_TT(void) {
    if (s_ttable == 0) {
        s_ttable = new TTable;
    }
    
    return s_ttable;
}

TTable::TTable(int size) {    
    m_buckets.resize(size);
}

void TTable::update(uint64 hash, const UCTNode * node) {        
    unsigned int index = (unsigned int)hash;
    
    index %= m_buckets.size();                             
    
    SMP::Lock lock(m_mutex);
    /*
        update TT
    */            
    m_buckets[index].m_hash       = hash;
    m_buckets[index].m_visits     = node->get_visits();
    m_buckets[index].m_blackwins  = node->get_blackwins();
    m_buckets[index].m_eval_sum   = node->get_eval_sum();
    m_buckets[index].m_eval_count = node->get_eval_count();
}

void TTable::sync(uint64 hash, UCTNode * node) {    
    unsigned int index = (unsigned int)hash;    
    
    index %= m_buckets.size();    
    
    SMP::Lock lock(m_mutex);
    /*
        check for hash fail
    */            
    if (m_buckets[index].m_hash != hash) {        
        return;
    }
    
    /*
        valid entry in TT should have more info than tree        
    */        
    if (m_buckets[index].m_visits > node->get_visits()) {    
        /*
            entry in TT has more info (new node)
        */            
        node->set_visits(m_buckets[index].m_visits);
        node->set_blackwins(m_buckets[index].m_blackwins);
        node->set_eval_sum(m_buckets[index].m_eval_sum);
        node->set_eval_count(m_buckets[index].m_eval_count);
    }     
}
