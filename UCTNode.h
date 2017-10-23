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
    using sortnode_t = std::tuple<float, int, UCTNode*>;

    explicit UCTNode(int vertex, float score);
    ~UCTNode();
    bool first_visit() const;
    bool has_children() const;
    bool create_children(std::atomic<int> & nodecount,
                         GameState & state, bool at_root);
    void kill_superkos(KoState & state);
    void delete_child(UCTNode * child);
    void invalidate();
    bool valid() const;
    int get_move() const;
    int get_visits() const;
    bool has_netscore() const;
    float get_score() const;
    void set_score(float score);
    float get_eval() const;
    float get_eval(int tomove) const;
    double get_blackevals() const;
    int get_evalcount() const;
    void set_move(int move);
    void set_visits(int visits);
    void set_blackevals(double blacevals);
    void set_evalcount(int evalcount);
    void set_eval(float eval);
    void accumulate_eval(float eval);
    void update(int color, bool update_eval, float eval = 0.5f);

    UCTNode* uct_select_child(int color, bool use_nets);
    UCTNode* get_first_child() const;
    UCTNode* get_pass_child() const;
    UCTNode* get_nopass_child() const;
    UCTNode* get_sibling() const;

    void sort_root_children(int color);
    void sort_children();
    SMP::Mutex & get_mutex();

private:
    UCTNode();
    void link_child(UCTNode * newchild);
    void link_nodelist(std::atomic<int> & nodecount,
                       FastBoard & state,
                       std::vector<Network::scored_node> & nodelist,
                       bool use_nets);
    void rescore_nodelist(std::atomic<int> & nodecount,
                         FastBoard & state,
                         std::vector<Network::scored_node> & nodelist,
                         bool all_symmetries);
    float smp_noise();
    // Tree data
    std::atomic<bool> m_has_children{false};
    UCTNode* m_firstchild{nullptr};
    UCTNode* m_nextsibling{nullptr};
    // Move
    int m_move;
    // UCT
    std::atomic<int> m_visits{0};
    // UCT
    float m_score;
    std::atomic<double> m_blackevals{0};
    std::atomic<int> m_evalcount{0};
    // alive (superko)
    std::atomic<bool> m_valid{true};
    // is someone adding scores to this node?
    bool m_is_expanding{false};
    bool m_is_netscoring{false};
    // dcnn node XXX redundant has_children
    bool m_has_netscore{false};
    int m_symmetries_done{0};
    SMP::Mutex m_nodemutex;
};

#endif
