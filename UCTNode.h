#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "config.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/atomic.hpp>

#include "SMP.h"
#include "GameState.h"
#include "Playout.h"
#include "Network.h"
#ifdef USE_OPENCL
#include "Network.h"
#endif

class UCTNode {
public:
    typedef boost::tuple<float, int, UCTNode*> sortnode_t;

    UCTNode(int vertex, float score, int expand_treshold);
    ~UCTNode();
    bool first_visit() const;
    bool has_children() const;
    float get_winrate(int tomove) const;
    float get_raverate() const;
    double get_blackwins() const;
    void create_children(boost::atomic<int> & nodecount,
                         FastState & state, bool use_nets, bool at_root);
    void expansion_cb(boost::atomic<int> * nodecount,
                      FastState & state,
                      Network::Netresult & netresult,
                      bool use_nets);
    void kill_superkos(KoState & state);
    void delete_child(UCTNode * child);
    void invalidate();
    bool valid();
    bool should_expand() const;
    int get_move() const;
    int get_visits() const;
    int get_ravevisits() const;
    float get_score() const;
    float get_eval() const;
    double get_eval_sum() const;
    int get_eval_count() const;
    int do_extend() const;
    bool has_eval_propagated() const;
    void set_eval_propagated();
    void set_best();
    void set_move(int move);
    void set_visits(int visits);
    void set_blackwins(double wins);
    void set_expand_cnt(int runs);
    void set_eval_sum(double eval_sum);
    void set_eval_count(int count);
    void update(Playout & gameresult, int color, bool update_eval);
    void updateRAVE(Playout & playout, int color);
    UCTNode* uct_select_child(int color, bool use_nets);
    UCTNode* get_first_child();
    UCTNode* get_pass_child();
    UCTNode* get_nopass_child();
    UCTNode* get_sibling();
    void sort_children(int color);
    SMP::Mutex & get_mutex();

private:
    void link_child(UCTNode * newchild);
    void link_nodelist(boost::atomic<int> & nodecount,
                       FastBoard & state,
                       std::vector<Network::scored_node> & nodes,
                       bool use_nets);

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
    // board eval
    bool m_eval_propagated;
    double m_eval_sum;
    int m_eval_count;
    // alive (superko)
    bool m_valid;
    // extend node
    int m_expand_cnt;
    bool m_is_expanding;
    // mutex
    SMP::Mutex m_nodemutex;
};

#endif
