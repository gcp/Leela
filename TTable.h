#ifndef TTABLE_H_INCLUDED
#define TTABLE_H_INCLUDED

#include <vector>

#include "UCTNode.h"
#include "SMP.h"

class TTEntry {
public:
    TTEntry()
        : m_hash(0) {
    };

    uint64 m_hash;

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
    void update(uint64 hash, const float komi, const UCTNode * node);

    /*
        sync given node with TT
    */
    void sync(uint64 hash, const float komi, UCTNode * node);

private:
    TTable(int size = 500000);

    SMP::Mutex m_mutex;
    std::vector<TTEntry> m_buckets;
    float m_komi;
};

#endif
