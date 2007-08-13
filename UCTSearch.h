#ifndef UCTSEARCH_H_INCLUDED
#define UCTSEARCH_H_INCLUDED

#include <memory>

#include "GameState.h"
#include "UCTNode.h"

class UCTSearch {
public:
    UCTSearch(GameState& g);
    int think(int color);
    
private:
    static const int MATURE_TRESHOLD = 1;    

    float play_simulation(UCTNode* node);    
    
    void dump_stats(GameState & state, UCTNode & parent);
    void dump_pv(GameState & state, UCTNode & parent);

    GameState m_rootstate;
    FastState m_currstate;
    UCTNode m_root;
    int m_nodes;
};

#endif