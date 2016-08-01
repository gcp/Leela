#ifndef TTABLE_H_INCLUDED
#define TTABLE_H_INCLUDED

#include <vector>

#include "UCTNode.h"
#include "SMP.h"

class TTEntry {
public:
    TTEntry()
        : m_hash(0), m_blackwins(0.0f), m_visits(0) {
    };

    uint64 m_hash;

    // XXX: need RAVE data here?
    double m_blackwins;
    int m_visits;
    double m_eval_sum;
    int m_eval_count;
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
    TTable(int size = 500000);

    std::vector<TTEntry> m_buckets;
    SMP::Mutex m_mutex;

    static TTable* s_ttable;
};

#endif
