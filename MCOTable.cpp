#include "config.h"
#include <cassert>

#include "MCOTable.h"
#include "FastBoard.h"
#include "Utils.h"

using Utils::atomic_add;

MCOwnerTable* MCOwnerTable::get_MCO() {
    static MCOwnerTable s_mcowntable;
    return &s_mcowntable;
}

MCOwnerTable::MCOwnerTable() {
    m_mcsimuls = 0;
    m_blackwins = 0;
    m_blackscore = 0.0f;
}

void MCOwnerTable::update_owns(Playout::bitboard_t & blacksq,
                               bool blackwon, float board_score) {
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
    atomic_add(m_blackscore, (double)board_score);
}

float MCOwnerTable::get_board_score() const {
    return (float)(m_blackscore / (double)m_mcsimuls);
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
    std::fill(begin(m_mcblackowner), end(m_mcblackowner), 0);
    std::fill(begin(m_mcwinowner), end(m_mcwinowner), 0);
    m_mcsimuls  = 0;
    m_blackwins = 0;
    m_blackscore = 0.0f;
}

bool MCOwnerTable::is_primed() const {
    return (get_MCO()->m_mcsimuls >= 32);
}
