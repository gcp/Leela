#include "config.h"

#include <vector>

#include "Utils.h"
#include "TTable.h"

TTable* TTable::get_TT(void) {
    static TTable s_ttable;
    return &s_ttable;
}

TTable::TTable(int size) {
    LOCK(m_mutex, lock);
    m_buckets.resize(size);
}

void TTable::clear(void) {
    LOCK(m_mutex, lock);
    std::fill(m_buckets.begin(), m_buckets.end(), TTEntry());
}

void TTable::update(uint64 hash, const UCTNode * node) {
    LOCK(m_mutex, lock);

    unsigned int index = (unsigned int)hash;
    index %= m_buckets.size();

    /*
        update TT
    */
    m_buckets[index].m_hash       = hash;
    m_buckets[index].m_visits     = node->get_visits();
    m_buckets[index].m_blackwins  = node->get_blackwins();
    m_buckets[index].m_eval_sum   = node->get_blackevals();
    m_buckets[index].m_eval_count = node->get_evalcount();
}

void TTable::sync(uint64 hash, UCTNode * node) {
    LOCK(m_mutex, lock);

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
        node->set_blackevals(m_buckets[index].m_eval_sum);
        node->set_evalcount(m_buckets[index].m_eval_count);
    }
}
