#include "config.h"

#include <vector>

#include "TTable.h"

TTable* TTable::s_ttable = 0;

TTable* TTable::get_TT(void) {
    if (s_ttable == 0) {
        s_ttable = new TTable;
    }
    
    return s_ttable;
}

TTable::TTable(int size) {
    boost::mutex::scoped_lock lock(m_mutex);
    m_buckets.resize(size);
}

void TTable::update(uint64 hash, const UCTNode * node) {    
    boost::mutex::scoped_lock lock(m_mutex);
    unsigned int index = (unsigned int)hash;
    
    index %= m_buckets.size();                             
    
    /*
        update TT
    */            
    m_buckets[index].m_hash      = hash;    
    m_buckets[index].m_visits    = node->get_visits();        
    m_buckets[index].m_blackwins = node->get_blackwins();
}

void TTable::sync(uint64 hash, UCTNode * node) {
    boost::mutex::scoped_lock lock(m_mutex);
    unsigned int index = (unsigned int)hash;    
    
    index %= m_buckets.size();    
    
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
    } 
}

TTEntry::TTEntry() 
: m_blackwins(0.0f), m_visits(0), m_hash(0) {
};
