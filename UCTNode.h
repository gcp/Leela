#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "GameState.h"

#include <vector>

class UCTNode {
public:    
    UCTNode();
    bool first_visit() const;
    bool has_children() const;
    float do_one_playout(FastState &startstate);
    float get_winrate(int tomove) const;
    void create_children(FastState &state);
    int get_move() const;
    int get_visits() const;
    void add_visit();
    void set_best();
    void set_move(int move);
    void update(float gameresult);
    
    typedef std::vector<UCTNode>::iterator iterator;
    typedef std::vector<UCTNode>::const_iterator const_iterator;
    iterator begin();
    iterator end();      

private:    
    UCTNode* m_parent;
    std::vector<UCTNode> m_children;   
    
    int m_bestchild; 
    
    int m_blackwins;
    int m_visits;
    int m_move;
};

#endif