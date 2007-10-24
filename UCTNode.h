#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "GameState.h"
#include "Playout.h"

class UCTNode {
public:        
    UCTNode(int vertex = FastBoard::PASS);
    ~UCTNode();    
    bool first_visit() const;
    bool has_children() const;    
    float get_winrate(int tomove) const;
    float get_raverate(int tomove) const;
    float get_blackwins() const;
    int create_children(FastState & state);
    void delete_child(UCTNode * child);        
    int get_move() const;
    int get_visits() const;
    int get_ravevisits() const;
    void set_best();
    void set_move(int move);
    void set_visits(int visits);
    void set_blackwins(float wins);
    void update(Playout & gameresult, int color); 
    void updateRAVE(Playout & playout, int color);
    void finalize(float gameresult);
    UCTNode* uct_select_child(int color);    
    UCTNode* get_first_child();
    UCTNode* get_pass_child();
    UCTNode* get_nopass_child();
    UCTNode* get_sibling();
    void sort_children(int color);        

private:                   
    void link_child(UCTNode * newchild);        
    
    // Tree data
    int m_move;       
    UCTNode* m_firstchild;
    UCTNode* m_nextsibling;                  
    // UCT
    float m_blackwins;
    int m_visits;      
    // RAVE
    float m_ravestmwins;    
    int m_ravevisits;  
};

#endif
