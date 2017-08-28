#include "config.h"
#include <cassert>
#include <vector>

#include "MCOTable.h"
#include "FastBoard.h"

MCOwnerTable* MCOwnerTable::get_MCO() {
    static MCOwnerTable s_mcowntable;
    return &s_mcowntable;
}

MCOwnerTable::MCOwnerTable() {
    m_mcsimuls = 0;
    m_blackwins = 0;
}

void MCOwnerTable::update_owns(Playout::bitboard_t & blacksq,
                               bool blackwon) {
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

int MCOwnerTable::get_blackown_i(const int color, const int vertex) const {
    assert(vertex >= 0 && vertex < FastBoard::MAXSQ);

    int owns = m_mcblackowner[vertex];
    int score = (1000 * owns) / m_mcsimuls;

    if (color == FastBoard::BLACK) {
        return score;
    } else {
        return 1000 - score;
    }
}

float MCOwnerTable::get_blackown(const int color, const int vertex) const {
    assert(vertex >= 0 && vertex < FastBoard::MAXSQ);

    float owns = (float)m_mcblackowner[vertex];
    float score = owns / (float)m_mcsimuls;

    if (color == FastBoard::BLACK) {
        return score;
    } else {
        return 1.0f - score;
    }
}

float MCOwnerTable::get_criticality_f(const int vtx) const {
    assert(vtx >= 0 && vtx < FastBoard::MAXSQ);

    double N = m_mcsimuls;
    double vx = m_mcwinowner[vtx];
    double term1 = (N - m_mcblackowner[vtx]);
    double term2 = (N - m_blackwins);
    double term3 = (m_mcblackowner[vtx]);
    double term4 = (m_blackwins);

    double term12 = ((term1/N) * (term2/N));
    double term34 = ((term3/N) * (term4/N));

    double crit = (vx/N) - (term12 + term34);
    crit = std::max<float>(0.0f, crit);

    assert(crit >= 0.0f && crit <= 0.51f);
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

bool MCOwnerTable::is_primed() const {
    return (get_MCO()->m_mcsimuls >= 32);
}
