#ifndef UCTSEARCH_H_INCLUDED
#define UCTSEARCH_H_INCLUDED

#include <memory>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>

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
#ifdef USE_OPENCL
    static constexpr int MCNN_MATURE_TRESHOLD = 75;
#else
    static constexpr int MCNN_MATURE_TRESHOLD = 250;
#endif
    static constexpr int MCTS_MATURE_TRESHOLD = 15;

    /*
        Maximum size of the tree in memory.
    */
    static constexpr int MAX_TREE_SIZE = 10000000;

    UCTSearch(GameState & g);
    int think(int color, passflag_t passflag = NORMAL);
    void set_playout_limit(int playouts);
    void set_use_nets(bool usenets);
    void set_runflag(boost::atomic<bool> * flag);
    void set_analyzing(bool flag);
    void set_quiet(bool flag);
    void ponder();
    bool is_running();
    Playout play_simulation(KoState & currstate, UCTNode * const node);
    float get_score();

private:
    void dump_stats(GameState & state, UCTNode & parent);
    std::string get_pv(GameState & state, UCTNode & parent);
    void dump_thinking();
    void dump_analysis();
    void dump_order2(void);
    int get_best_move(passflag_t passflag);
    int get_best_move_nosearch(std::vector<std::pair<float, int>> moves,
                               float score, passflag_t passflag);
    bool allow_early_exit();
    bool easy_move();

    GameState & m_rootstate;
    UCTNode m_root;
    boost::atomic<int> m_nodes;
    boost::atomic<bool> m_run;
    int m_maxplayouts;
    float m_score;

    // For external control
    bool m_hasrunflag;
    boost::atomic<bool> * m_runflag;

    // Special modes
    bool m_use_nets;
    bool m_analyzing;
    bool m_quiet;
};

class UCTWorker {
public:
    UCTWorker(GameState & state, UCTSearch * search, UCTNode * root)
      : m_rootstate(state), m_search(search), m_root(root) {};
    void operator()();
private:
    GameState & m_rootstate;
    UCTSearch * m_search;
    UCTNode * m_root;
};

#endif
