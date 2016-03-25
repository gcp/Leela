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
    m_mcblackowner.resize(FastBoard::MAXSQ);
    m_mcwinowner.resize(FastBoard::MAXSQ);
    m_mcsimuls = 0;
    m_blackwins = 0;
}

void MCOwnerTable::update_owns(Playout::bitboard_t & blacksq, 
                               bool blackwon) {
    SMP::Lock lock(m_mutex);
    for (size_t i = 0; i < blacksq.size(); i++) {
        if (blacksq[i]) {
            m_mcblackowner[i]++;
            if (blackwon) {
                m_mcwinowner[i]++;
            } 
        } else {
            if (!blackwon) {
                m_mcwinowner[i]++;
            } 
        }
    }    
    m_mcsimuls++;
    if (blackwon) {
        m_blackwins++;
    }
}

int MCOwnerTable::get_blackown_i(const int color, const int vertex) {    
    assert(vertex >= 0 && vertex < FastBoard::MAXSQ);
    
    int owns = m_mcblackowner[vertex];
    int score = (1000 * owns) / m_mcsimuls;
    
    if (color == FastBoard::BLACK) {
        return score;
    } else {
        return 1000 - score;
    }
}

float MCOwnerTable::get_blackown(const int color, const int vertex) {    
    assert(vertex >= 0 && vertex < FastBoard::MAXSQ);
    
    float owns = (float)m_mcblackowner[vertex];
    float score = owns / (float)m_mcsimuls;
    
    if (color == FastBoard::BLACK) {
        return score;
    } else {
        return 1.0f - score;
    }
}

float MCOwnerTable::get_criticality_f(const int vtx) {
    assert(vtx >= 0 && vtx < FastBoard::MAXSQ);

    float N = m_mcsimuls;
    float vx = m_mcwinowner[vtx];
    float term1 = (N - m_mcblackowner[vtx]);
    float term2 = (N - m_blackwins);
    float term3 = (m_mcblackowner[vtx]);
    float term4 = (m_blackwins);

    float term12 = ((term1/N) * (term2/N));
    float term34 = ((term3/N) * (term4/N));

    float crit = (vx/N) - (term12 + term34);
    crit = std::max<float>(0.0f, crit);

    assert(crit >= 0.0f && crit <= 0.51f);
    //if (crit < -500 || crit > 500) {
    //    assert(false);
    //}
    return crit;
}

int MCOwnerTable::get_criticality_i(const int vtx) {
    assert(vtx >= 0 && vtx < FastBoard::MAXSQ);

    int N = m_mcsimuls;
    int vx = m_mcwinowner[vtx];
    int term1 = (N - m_mcblackowner[vtx]);
    int term2 = (N - m_blackwins);
    int term3 = (m_mcblackowner[vtx]);
    int term4 = (m_blackwins);

    int term12 = (term1 * term2) / N;
    int term34 = (term3 * term4) / N;

    int crit = vx - (term12 + term34);
    crit = (crit * 1000) / N;

    crit = std::max(-500, crit);
    //if (crit < -500 || crit > 500) {
    //    assert(false);
    //}

    return crit;
}

void MCOwnerTable::clear() {     
    std::fill(get_MCO()->m_mcblackowner.begin(), get_MCO()->m_mcblackowner.end(), 0);    
    std::fill(get_MCO()->m_mcwinowner.begin(), get_MCO()->m_mcwinowner.end(), 0);
    get_MCO()->m_mcsimuls  = 0;
    get_MCO()->m_blackwins = 0;
}

bool MCOwnerTable::is_primed() {
    return (get_MCO()->m_mcsimuls >= 32);
}
