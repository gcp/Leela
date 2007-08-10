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
    int play_simulation(UCTNode &node);    

    GameState m_rootstate;
    GameState m_currstate;
    UCTNode m_root;
    int m_nodes;
};

#endif