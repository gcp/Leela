#ifndef PNNODE_H_INCLUDED
#define PNNODE_H_INCLUDED

#include "config.h"

#include <vector>

#include "KoState.h"

class PNNode {
public:    
    static const int INF = 10000000;

    enum node_type_t { AND = 0, OR = 1 };

    PNNode();
    PNNode(PNNode * m_parent, int move);    
    void evaluate(KoState * ks, int move, int rootcolor, int groupid, int maxnodes = 0);
    void set_proof_disproof(node_type_t type);
    bool solved();
    PNNode * select_most_proving(KoState * ks, node_type_t type);
    PNNode * select_critical(node_type_t type);
    int develop_node(KoState * ks, int rootcolor, int groupid, int maxnodes = 0);
    void update_ancestors(node_type_t type);

    int get_proof() const;
    int get_disproof() const;
    int get_move() const;
    bool is_expanded() const;
    bool has_children() const;

private:
    int m_pn;
    int m_dn;
    int m_move;

    bool m_evaluated;
    bool m_expanded;    

    PNNode * m_parent;    
    std::vector<PNNode> m_children;
};

#endif