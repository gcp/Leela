#include <vector>

#include "FastState.h"
#include "PNSearch.h"
#include "Utils.h"

using namespace Utils;

PNSearch::PNSearch(KoState & ks) 
    : m_state(ks) {    
}

void PNSearch::classify_groups() {    
    int size = m_state.board.get_boardsize();

    // Need separate "seen" and "unknown" markers to
    // prevent rescanning unknown groups
    std::vector<bool> groupmarker(FastBoard::MAXSQ, false);
    std::vector<status_t> groupstatus(FastBoard::MAXSQ, UNKNOWN);

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int vtx = m_state.board.get_vertex(i, j);
            int sq = m_state.board.get_square(vtx);

            if (sq < FastBoard::EMPTY) {
                int par = m_state.board.get_groupid(vtx);
                if (!groupmarker[par]) {
                    if (m_state.board.string_size(par) > 1) {
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
    std::string groupname = m_state.board.move_to_text(groupid);
    myprintf("Scanning group %s\n", groupname.c_str());
    
    

    return UNKNOWN;
}
