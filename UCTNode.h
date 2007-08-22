#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "GameState.h"

class UCTNode {
public:        
    UCTNode(int vertex = -1);
    ~UCTNode();    
    bool first_visit() const;
    bool has_children() const;
    float do_one_playout(FastState &startstate);
    float get_winrate(int tomove) const;    
    void create_children(FastState &state);
    int get_move() const;
    int get_visits() const;    
    void set_best();    
    void set_move(int move);
    void update(float gameresult); 
    UCTNode* uct_select_child(int color);    
    UCTNode* get_first_child();
    UCTNode* get_pass_child();
    UCTNode* get_nopass_child();
    UCTNode* get_sibling();
    void sort_children(int color);    

private:                   
    void link_child(UCTNode * newchild);    

    // keep this, move, and put rest in hash?
    UCTNode* m_firstchild;
    UCTNode* m_nextsibling;       
    
    //float m_square;
    float m_blackwins;
    int m_visits;
    int m_move;            
};

#endif