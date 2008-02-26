#include <vector>

#include "FastState.h"
#include "PNSearch.h"
#include "Utils.h"

using namespace Utils;

PNSearch::PNSearch(KoState & ks) 
    : m_rootstate(ks) {    
}

void PNSearch::classify_groups() {    
    int size = m_rootstate.board.get_boardsize();

    // Need separate "seen" and "unknown" markers to
    // prevent rescanning unknown groups
    std::vector<bool> groupmarker(FastBoard::MAXSQ, false);
    std::vector<status_t> groupstatus(FastBoard::MAXSQ, UNKNOWN);

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int vtx = m_rootstate.board.get_vertex(i, j);
            int sq = m_rootstate.board.get_square(vtx);

            if (sq < FastBoard::EMPTY) {
                int par = m_rootstate.board.get_groupid(vtx);
                if (!groupmarker[par]) {
                    if (m_rootstate.board.string_size(par) > 1) {
                        status_t status = check_group(par);
                        groupstatus[par] = status;
                        groupmarker[par] = true;
                    }
                }
            }
        }
    }
}

PNSearch::status_t PNSearch::check_group(int groupid) {
    std::string groupname = m_rootstate.board.move_to_text(groupid);
    myprintf("Scanning group %s\n", groupname.c_str());

    m_root.reset(new PNNode());

    m_group_to_check = groupid;
    m_group_color = m_rootstate.board.get_square(groupid);

    int rootcolor = m_rootstate.get_to_move();
    
    m_root->evaluate(&m_rootstate, m_group_color, m_group_to_check);    

    int iters = 0;
    while(!m_root->solved() && ++iters < 50000) {
        m_workstate = m_rootstate;
        PNNode * most_proving = m_root->select_most_proving(&m_workstate, PNNode::OR);        
        most_proving->develop_node(&m_workstate, m_group_color, m_group_to_check);        
        most_proving->update_ancestors(m_workstate.get_to_move() == rootcolor ? PNNode::OR : PNNode::AND);               
        if ((iters & 1023) == 0) {
            m_workstate = m_rootstate;
            std::string pv = get_pv(&m_workstate, &(*m_root));
            myprintf("P: %d D: %d Iter: %d PV: %s\n", m_root->get_proof(), m_root->get_disproof(), iters, pv.c_str());
        }
    }

    myprintf("P: %d D: %d Iter: %d\n", m_root->get_proof(), m_root->get_disproof(), iters);

    return UNKNOWN;
}

std::string PNSearch::get_pv(KoState * state, PNNode * node) {    
    std::string res("");    

    PNNode * oldnode = NULL;
    PNNode * critical = node;
    PNNode::node_type_t type = PNNode::OR;

    do {
        oldnode = critical;        
        critical = critical->select_critical(state, type);    
        int move = critical->get_move();        
        //state->play_move(move);
        std::string mtxt = state->board.move_to_text(move);        

        if (type == PNNode::OR) {
            type = PNNode::AND;
        } else {
            type = PNNode::OR;
        }

        res.append(mtxt);
        res.append(" ");
    } while (oldnode != critical && critical->is_expanded());

    return res;
}