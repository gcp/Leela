#ifndef UCTSEARCH_H_INCLUDED
#define UCTSEARCH_H_INCLUDED

#include <memory>

#include "GameState.h"
#include "UCTNode.h"
#include "Playout.h"

class UCTSearch {
public:
    /*
        Depending on rule set and state of the game, we might
        prefer to pass, or we might prefer not to pass unless
        it's the last resort.
    */        
    typedef enum { 
        NORMAL = 0, PREFERPASS = 1, NOPASS = 2
    } passflag_t;    
    
    /*
        Don't expand children until at least this many
        visits happened.
    */        
    static const int MATURE_TRESHOLD = 30;     
    
    UCTSearch(GameState & g);
    int think(int color, passflag_t passflag = NORMAL);
    
private:     
    Playout play_simulation(UCTNode* node);    
    
    void dump_stats(GameState & state, UCTNode & parent);
    void dump_pv(GameState & state, UCTNode & parent);
    void dump_thinking();  
    void dump_history(void);
    void dump_order(void);
    void dump_order2(void);
    int get_best_move(passflag_t passflag);

    GameState & m_rootstate;
    KoState m_currstate;
    UCTNode m_root;    
    int m_nodes;       
};

#endif
