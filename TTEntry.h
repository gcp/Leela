#ifndef TTENTRY_H_INCLUDED
#define TTENTRY_H_INCLUDED

#include "config.h"

class TTEntry {
public:
    TTEntry();
    
    uint64 m_hash;

    float m_blackwins;
    int m_visits;
};

#endif