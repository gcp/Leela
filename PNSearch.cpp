#include <vector>

#include "FastState.h"
#include "FastBoard.h"
#include "PNSearch.h"
#include "Utils.h"
#include "Timing.h"

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

                        // find the entire chain for this stone
                        std::string chain_list;
                        std::vector<int> chains;
                        m_rootstate.board.augment_chain(chains, vtx);                        

                        // mark the entire augmented chain
                        for (int i = 0; i < chains.size(); i++) {
                            groupstatus[chains[i]] = status;
                            groupmarker[chains[i]] = true;
                            std::string groupname = m_rootstate.board.move_to_text(chains[i]);
                            chain_list.append(groupname);
                            chain_list.append(" ");                            
                        }
                        myprintf("Marking %sas seen\n", chain_list.c_str());
                    }                    
                }                
            }
        }
    }
}

std::pair<int,int> PNSearch::do_search(int groupid, int maxnodes) {
    m_root.reset(new PNNode());        

    m_group_color = m_rootstate.board.get_square(groupid);            
    int rootcolor = m_rootstate.get_to_move();    
   
    // no need as it happened in the father tree
    m_root->evaluate(&m_rootstate, FastBoard::PASS, m_group_color, groupid);    

    int iters = 0;
    while(!m_root->solved() && iters++ < maxnodes) {                
        m_workstate = m_rootstate;
        PNNode * most_proving = m_root->select_most_proving(&m_workstate,
                                                             m_workstate.get_to_move() == m_group_color ? 
                                                             PNNode::OR : PNNode::AND);        
        most_proving->develop_node(&m_workstate, m_group_color, groupid);        
        most_proving->update_ancestors(m_workstate.get_to_move() == m_group_color  ? 
                                       PNNode::OR : PNNode::AND);                       
    } 

    return std::pair<int,int>(m_root->get_proof(), m_root->get_disproof());
}

PNSearch::status_t PNSearch::check_group(int groupid) {
    std::string groupname = m_rootstate.board.move_to_text(groupid);
    myprintf("Scanning group %s\n", groupname.c_str());

    m_root.reset(new PNNode());        
            
    m_group_color = m_rootstate.board.get_square(groupid);            
    int rootcolor = m_rootstate.get_to_move();    
    
    // start Proof number search
    m_root->evaluate(&m_rootstate, FastBoard::PASS, m_group_color, groupid);   
    
    int nodes = 1;     
    int iters = 0;
    int last_update = 0;
    
    Time start;
    
    while(!m_root->solved() && ++iters < 5000) {                
        m_workstate = m_rootstate;
        PNNode * most_proving = m_root->select_most_proving(&m_workstate,
                                                             m_workstate.get_to_move() == m_group_color ? 
                                                             PNNode::OR : PNNode::AND);        
        nodes += most_proving->develop_node(&m_workstate, m_group_color, groupid, nodes); 
        most_proving->update_ancestors(m_workstate.get_to_move() == m_group_color  ? 
                                       PNNode::OR : PNNode::AND);               
                                       
        Time elapsed;
        int centiseconds_elapsed = Time::timediff(start, elapsed);        
           
         if (centiseconds_elapsed - last_update > 100) {
            last_update = centiseconds_elapsed;            
            m_workstate = m_rootstate;
            std::string pv = get_pv(&m_workstate, &(*m_root));
            myprintf("P: %d D: %d N: %d Iter: %d PV: %s\n",   m_root->get_proof(), 
                                                              m_root->get_disproof(), 
                                                              nodes,
                                                              iters, pv.c_str());		
        }                   
    }

    m_workstate = m_rootstate;
    std::string pv = get_pv(&m_workstate, &(*m_root));
    myprintf("P: %d D: %d Iter: %d PV: %s\n", m_root->get_proof(), m_root->get_disproof(), iters, pv.c_str());

    return UNKNOWN;
}

std::string PNSearch::get_pv(KoState * state, PNNode * node) {    
    std::string res("");    

    PNNode * oldnode = NULL;
    PNNode * critical = node;
    PNNode::node_type_t type = state->get_to_move() == m_group_color  ? 
                               PNNode::OR : PNNode::AND;

    do {
        if (!critical->has_children()) break;

        oldnode = critical;               
        critical = critical->select_critical(type);            
        int move = critical->get_move();                
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
