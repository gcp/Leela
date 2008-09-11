#include "config.h"
#include <cassert>
#include <vector>

#include "MCOTable.h"
#include "FastBoard.h"

MCOwnerTable* MCOwnerTable::s_mcowntable = 0;

MCOwnerTable* MCOwnerTable::get_MCO() {
    if (s_mcowntable == 0) {
        s_mcowntable = new MCOwnerTable;            
    }
    
    return s_mcowntable;
}

MCOwnerTable::MCOwnerTable() {
    m_mcowner.resize(FastBoard::MAXSQ);
}

void MCOwnerTable::update(Playout::bitboard_t & blacksq) {  
    SMP::Lock lock(m_mutex);
    for (int i = 0; i < blacksq.size(); i++) {
        if (blacksq[i]) {
            m_mcowner[i]++;
        } 
    }    
    m_mcsimuls++;
}

float MCOwnerTable::get_score(const int color, const int vertex) {    
    assert(vertex >= 0 && vertex < FastBoard::MAXSQ);
    
    float owns = (float)m_mcowner[vertex];
    float score = owns / (float)m_mcsimuls;
    
    if (color == FastBoard::BLACK) {
        return score;
    } else {
        return 1.0f - score;
    }
}

void MCOwnerTable::clear() {     
    std::fill(get_MCO()->m_mcowner.begin(), get_MCO()->m_mcowner.end(), 0);    
    get_MCO()->m_mcsimuls = 0;
}

bool MCOwnerTable::is_primed() {
    return (get_MCO()->m_mcsimuls >= 32);
}
