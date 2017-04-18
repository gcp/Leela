#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "config.h"

#include <tuple>
#include <atomic>

#include "SMP.h"
#include "GameState.h"
#include "Playout.h"
#include "Network.h"

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
                    Network::Netresult & raw_netlist,
                    bool all_symmetries);
    void run_value_net(FastState & state);
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
    float get_eval(int tomove) const;
    float get_mixed_score(int tomove);
    double get_blackevals() const;
    int get_evalcount() const;
    bool has_eval_propagated() const;
    void set_eval_propagated();
    void set_move(int move);
    void set_visits(int visits);
    void set_blackwins(double wins);

    void set_expand_cnt(int runs, int netscore_runs);
    void set_blackevals(double blacevals);
    void set_evalcount(int evalcount);
    void set_expand_cnt(int runs);
    void set_eval(float eval);
    void accumulate_eval(float eval);
    void update(Playout & gameresult, int color, bool update_eval);
    void updateRAVE(Playout & playout, int color);

    UCTNode* uct_select_child(int color, bool use_nets);
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
                       Network::Netresult & nodes,
                       bool use_nets);
    void rescore_nodelist(std::atomic<int> & nodecount,
                         FastBoard & state,
                         Network::Netresult & nodes,
                         bool all_symmetries);
    float smp_noise();
    // Tree data
    UCTNode* m_firstchild;
    UCTNode* m_nextsibling;
    // Move
    int m_move;
    // UCT
    std::atomic<double> m_blackwins;
    std::atomic<int> m_visits;
    // RAVE
    std::atomic<double> m_ravestmwins;
    std::atomic<int> m_ravevisits;
    // move order
    float m_score;
    // board eval
    bool m_eval_propagated;
    std::atomic<double> m_blackevals;
    std::atomic<int> m_evalcount;
    bool m_is_evaluating;    // mutex
    // alive (superko)
    bool m_valid;
    // extend node
    int m_expand_cnt;
    bool m_is_expanding;
    // dcnn node
    bool m_has_netscore;
    int m_netscore_thresh;
    int m_symmetries_done;
    bool m_is_netscoring;
    SMP::Mutex m_nodemutex;
};

#endif
