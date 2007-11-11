#ifndef UCTSEARCH_H_INCLUDED
#define UCTSEARCH_H_INCLUDED

#include <memory>
#include <boost/function.hpp>
#include <boost/thread.hpp>

#include "GameState.h"
#include "UCTNode.h"
#include "Playout.h"

class UCTSearch {
public:
    /*
        Depending on rule set and state of the game, we might
        prefer to pass, or we might prefer not to pass unless
        it's the last resort. Same for resigning.
    */        
    typedef int passflag_t;
    static const passflag_t NORMAL   = 0;
    static const passflag_t NOPASS   = 1 << 0;
    static const passflag_t NORESIGN = 1 << 1;
    
    /*
        Don't expand children until at least this many
        visits happened.
    */        
    static const int MATURE_TRESHOLD = 30; 
    /*
        Maximum size of the tree in memory.
    */        
    static const int MAX_TREE_SIZE = 2000000;    
    
    UCTSearch(GameState & g);
    int think(int color, passflag_t passflag = NORMAL);
    void ponder();    
    bool is_running();      
    Playout play_simulation(KoState & currstate, UCTNode * node);
    
private:             
    void dump_stats(GameState & state, UCTNode & parent);
    void dump_pv(GameState & state, UCTNode & parent);
    void dump_thinking();        
    void dump_order2(void);
    int get_best_move(passflag_t passflag);    

    GameState & m_rootstate;    
    UCTNode m_root;    
    int m_nodes;  
    bool m_run;        
};

class UCTWorker {
public:
    UCTWorker(GameState & state, UCTSearch * search, UCTNode * root)
      : m_rootstate(state), m_search(search), m_root(root) {};
    void operator()();
private:
    GameState & m_rootstate; 
    UCTNode * m_root;
    UCTSearch * m_search;        
};

#endif
