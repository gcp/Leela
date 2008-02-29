#ifndef PNSEARCH_H_INCLUDED
#define PNSEARCH_H_INCLUDED

#include "config.h"

#include <string>

#include "FastBoard.h"
#include "KoState.h"
#include "PNNode.h"

class PNSearch {
    enum status_t { UNKNOWN = 0, DEAD = 1, ALIVE = 2 };

public:
    PNSearch(KoState & ks);
    void classify_groups();
    status_t check_group(int groupid);      

private:
    std::string get_pv(KoState * state, PNNode * node);

    KoState m_rootstate;
    KoState m_workstate;
    int m_group_to_check;
    int m_group_color;        

    std::auto_ptr<PNNode> m_root;    
};

#endif