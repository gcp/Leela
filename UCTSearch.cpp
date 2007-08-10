#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"

#include "FastBoard.h"
#include "UCTSearch.h"
#include "Timing.h"

UCTSearch::UCTSearch(GameState &g)
: m_rootstate(g) {
    
}

int UCTSearch::play_simulation(UCTNode &node) {
    float noderesult;

    if (node.first_visit()) {
        noderesult = node.do_one_playout(m_currstate);
    } else {
        if (node.m_children.size() == 0) {
            node.create_children(m_currstate);
        }

        UCTNode* next = UCTSelect(n); 

        m_currstate.makeMove(next.move);
        playSimulation(next);
    }

      n.visits++;
        updateWin(n, randomresult);

        if (n.child!=null)
            n.setBest();


}

int UCTSearch::think(int color) {
    Time start;
    int time_for_move = 100;

    m_nodes = 0;
    m_currstate = m_rootstate;
    
    int centiseconds_elapsed;
    
    do {
        play_simulation(m_root);

        m_nodes++;

        Time elapsed;
        centiseconds_elapsed = Time::timediff(start, elapsed);
    } while (centiseconds_elapsed < time_for_move);

    return FastBoard::PASS;
}