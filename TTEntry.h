#ifndef TTENTRY_H_INCLUDED
#define TTENTRY_H_INCLUDED

#include "config.h"

class TTEntry {
public:
    uint64 hash;

    int m_blackwins;
    int m_visits;
};

#endif