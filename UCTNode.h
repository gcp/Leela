#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "GameState.h"

#include <vector>

class UCTNode {
public:    
    UCTNode();
    bool first_visit();
    bool has_children();
    float do_one_playout(FastState &startstate);
    float get_winrate(int tomove);
    void create_children(FastState &state);
    int get_move();
    int get_visits();
    void add_visit();
    void set_best();
    void set_move(int move);
    void update(float gameresult);
    
    std::vector<UCTNode>::iterator first_child();
    std::vector<UCTNode>::iterator end_child();

private:    
    UCTNode* m_parent;
    std::vector<UCTNode> m_children;   
    
    int m_bestchild; 
    
    int m_blackwins;
    int m_visits;
    int m_move;
};

#endif