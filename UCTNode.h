#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "config.h"

#include <tuple>
#include <atomic>

#include "SMP.h"
#include "GameState.h"
#include "Playout.h"
#include "Network.h"
#ifdef USE_OPENCL
#include "Network.h"
#endif

class UCTNode {
public:
    typedef std::tuple<float, int, UCTNode*> sortnode_t;

    UCTNode(int vertex, float score,
            int expand_threshold, int netscore_threshold);
    ~UCTNode();
    bool first_visit() const;
    bool has_children() const;
    float get_winrate(int tomove) const;
    float get_raverate() const;
    double get_blackwins() const;
    void create_children(std::atomic<int> & nodecount,
                         FastState & state, bool at_root, bool use_nets);
    void netscore_children(std::atomic<int> & nodecount,
                           FastState & state, bool at_root);
    void scoring_cb(std::atomic<int> * nodecount,
                    FastState & state,
                    std::vector<Network::scored_node> & raw_netlist);
    void kill_superkos(KoState & state);
    void delete_child(UCTNode * child);
    void invalidate();
    bool valid();
    bool should_expand() const;
    bool should_netscore() const;
    int get_move() const;
    int get_visits() const;
    int get_ravevisits() const;
    bool has_netscore() const { return m_has_netscore; }
    float get_score() const;
    void set_score(float score);
    void set_best();
    void set_move(int move);
    void set_visits(int visits);
    void set_blackwins(double wins);
    void set_expand_cnt(int runs, int netscore_runs);
    void update(Playout & gameresult, int color);
    void updateRAVE(Playout & playout, int color);
    UCTNode* uct_select_child(int color);
    UCTNode* get_first_child();
    UCTNode* get_pass_child();
    UCTNode* get_nopass_child();
    UCTNode* get_sibling();
    void sort_root_children(int color);
    void sort_children();
    SMP::Mutex & get_mutex();

private:
    void link_child(UCTNode * newchild);
    void link_nodelist(std::atomic<int> & nodecount,
                       FastBoard & state,
                       std::vector<Network::scored_node> & nodes,
                       bool use_nets);
    void rescore_nodelist(std::atomic<int> & nodecount,
                         FastBoard & state,
                         std::vector<Network::scored_node> & nodes);
    // Tree data
    UCTNode* m_firstchild;
    UCTNode* m_nextsibling;
    // Move
    int m_move;
    // UCT
    double m_blackwins;
    int m_visits;
    // RAVE
    double m_ravestmwins;
    int m_ravevisits;
    // move order
    float m_score;
    // alive (superko)
    bool m_valid;
    // extend node
    int m_expand_cnt;
    bool m_is_expanding;
    // dcnn node
    bool m_has_netscore;
    int m_netscore_cnt;
    bool m_is_netscoring;
    // mutex
    SMP::Mutex m_nodemutex;
};

#endif
