#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "GameState.h"

class UCTNode {
public:    
    bool first_visit();
    float do_one_playout(FastState &startstate);
    void create_children(FastState &state);

private:    
    UCTNode* m_parent;
    std::vector<UCTNode> m_children;    

    int m_blackwins;
    int m_visits;
    int m_move;
};

#endif