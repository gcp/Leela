#include "config.h"

#include <vector>

#include "HistoryTable.h"
#include "FastBoard.h"

HistoryTable* HistoryTable::s_htable = 0;

HistoryTable* HistoryTable::get_HT() {
    if (s_htable == 0) {
        s_htable = new HistoryTable;            
    }
    
    return s_htable;
}

HistoryTable::HistoryTable() {
    m_entries.resize(FastBoard::MAXSQ);
}

void HistoryTable::update(int vertex, bool win) {
    if (vertex == FastBoard::PASS) {
        vertex = 0;
    } 
    if (win) {
        m_entries[vertex].m_wins++;
    }
    m_entries[vertex].m_visits++;    
}

float HistoryTable::get_score(int vertex) {    
    if (vertex == FastBoard::PASS) {
        vertex = 0;
    } 
    
    assert(m_entries[vertex].m_visits > 0);
    
    float wins = (float)m_entries[vertex].m_wins / (float)m_entries[vertex].m_visits;    

    return wins;
}

int HistoryTable::get_visits(int vertex) {
    if (vertex == FastBoard::PASS) {
        vertex = 0;
    } 
    return m_entries[vertex].m_visits;
}

void HistoryTable::clear() {
    HistoryEntry empty;    
    std::fill(get_HT()->m_entries.begin(), get_HT()->m_entries.end(), empty);    
}

HistoryEntry::HistoryEntry()
: m_visits(2), m_wins(1) {
}
