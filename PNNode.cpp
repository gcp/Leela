#include "config.h"

#include "Attributes.h"
#include "PNNode.h"
#include "FastBoard.h"

PNNode::PNNode(PNNode * parent, int move) 
    : m_parent(parent), m_move(move) {
    m_pn = 1;
    m_dn = 1;
    m_evaluated = false;
    m_expanded = false;
}

PNNode::PNNode() {  
    m_parent = NULL;
    m_move = FastBoard::PASS;
    m_pn = 1;
    m_dn = 1;
    m_evaluated = false;
    m_expanded = false;
}

void PNNode::evaluate(FastState * ks, int groupcolor, int groupid) {
    if (m_evaluated) {
        return;
    }

    m_evaluated = true;
    
    // group removed
    if (ks->board.get_square(groupid) != groupcolor) {
        m_pn = INF;
        m_dn = 0;
        return;
    }
    
    // excessive liberties
    /*if (ks->board.count_rliberties(groupid) >= 8) {
        m_pn = 0;
        m_dn = INF;
        return;
    }*/

    bool alive = ks->board.is_alive(groupid);

    // group alive by 2 eyes
    if (alive) {
        //ks->display_state();
        m_pn = 0;
        m_dn = INF;
        return;
    }
    
    // group alive by passout
    if (ks->get_passes() >= 2) {
        m_pn = 0;
        m_dn = INF;
        return;
    }

    // still fighting, set heuristics
    m_pn = 1;
    m_dn = 1;
    //m_pn = std::max(1, 7 - ks->board.count_rliberties(groupid));
    //m_dn = std::max(1, ks->board.count_rliberties(groupid)); 
}

void PNNode::set_proof_disproof(node_type_t type) {
    assert(m_evaluated);
    assert(m_children.size() > 0);
    if (m_expanded) {    
        int proof;
        int disproof;

        if (type == AND) {
            proof = 0;
            disproof = INF;
        } else {
            proof = INF;
            disproof = 0;
        }

        for (int i = 0; i < m_children.size(); i++) {
            if (type == AND) {
                proof += m_children[i].m_pn;
                disproof = std::min(disproof, m_children[i].m_dn);
            } else {
                proof = std::min(proof, m_children[i].m_pn);
                disproof += m_children[i].m_dn;                
            }
        }  

        proof = std::min(proof, INF);
        disproof = std::min(disproof, INF);

        m_pn = proof;
        m_dn = disproof;
    } 
}

bool PNNode::solved() {
    if (m_pn == 0 || m_dn == 0) {
            return true;
    }
    return false;
}

PNNode * PNNode::select_critical(node_type_t type) {
    assert(m_children.size() > 0);
    int i = 0;
    if (type == OR) {            
        while (m_children[i].m_pn != m_pn) i++;
    } else {            
        while (m_children[i].m_dn != m_dn) i++;
    }
    return &(m_children[i]);
}

PNNode * PNNode::select_most_proving(KoState * ks, node_type_t type) {
    PNNode * res = this;

    if (m_expanded) {
        PNNode * critical = select_critical(type); 
        ks->play_move(critical->m_move);
        res = critical->select_most_proving(ks, type == AND ? OR : AND);        
    }

    return res;
}

void PNNode::develop_node(KoState * ks, std::vector<bool> & roi, int groupcolor, int groupid) {    
    std::vector<int> moves = ks->generate_moves(ks->get_to_move());

    for (int i = 0; i < moves.size(); i++) {       
        if (moves[i] != FastBoard::PASS) {
            //std::vector<int> nbrs = ks->board.get_neighbour_ids(moves[i]);
            //std::vector<int>::iterator it = std::find(nbrs.begin(), nbrs.end(), groupid);

            if (/*it != nbrs.end() ||*/ roi[moves[i]]) {
                PNNode node(this, moves[i]);
                m_children.push_back(node);        
            }
        }        
    }
    
    // allow pass if there are 2 or less good options (could be eye fills
    // for the defender)
    // if attacker has no good moves and there is already a pass, 
    // set_proof_disproof knows that the group is alive
    if (m_children.size() <= 2) {
        m_children.push_back(PNNode(this, FastBoard::PASS));    
    }    

    //if (m_children.size() == 0) {
    //    ks->display_state();        
    //}
        
    for (int i = 0; i < m_children.size(); i++) {
        KoState tmp = *ks;        
        tmp.play_move(m_children[i].m_move);
        if (m_children[i].m_move == FastBoard::PASS || !tmp.superko()) {
            m_children[i].evaluate(&tmp, groupcolor, groupid);     
        } else {
            // neither player can win a ko fight            
            if (tmp.get_to_move() == groupcolor) {
                // illegal move was made by attacker
                // group is safe
                m_children[i].m_pn = 0;
                m_children[i].m_dn = INF;
                m_children[i].m_evaluated = true;
            } else if (tmp.get_to_move() == !groupcolor) {
                // illegal move made by defender
                // group is dead
                m_children[i].m_pn = INF;
                m_children[i].m_dn = 0;
                m_children[i].m_evaluated = true;
            }            
        }
    } 

    m_expanded = true;
}

void PNNode::update_ancestors(node_type_t type) {
    set_proof_disproof(type);

    if (m_parent != NULL) {
        m_parent->update_ancestors(type == AND ? OR : AND);
    }
}

int PNNode::get_proof() const {
    return m_pn;
}

int PNNode::get_disproof() const {
    return m_dn;
}

int PNNode::get_move() const {
    return m_move;
}

bool PNNode::is_expanded() const {
    return m_expanded;
}

bool PNNode::has_children() const {
    return m_children.size() > 0;
}