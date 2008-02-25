#ifndef PNSEARCH_H_INCLUDED
#define PNSEARCH_H_INCLUDED

#include "config.h"

#include "FastBoard.h"
#include "KoState.h"

class PNSearch {
    enum status_t { UNKNOWN = 0, DEAD = 1, ALIVE = 2 };

public:
    PNSearch(KoState & ks);
    void classify_groups();
    status_t check_group(int groupid);

private:
    KoState m_state;
};

#endif