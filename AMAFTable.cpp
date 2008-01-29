#include "config.h"

#include <vector>

#include "AMAFTable.h"
#include "FastBoard.h"

AMAFTable* AMAFTable::s_amaftable = 0;

AMAFTable* AMAFTable::get_AMAFT() {
    if (s_amaftable == 0) {
        s_amaftable = new AMAFTable;            
    }
    
    return s_amaftable;
}

AMAFTable::AMAFTable() {
    m_wins[0].resize(FastBoard::MAXSQ);
    m_wins[1].resize(FastBoard::MAXSQ);
    m_visits[0].resize(FastBoard::MAXSQ);
    m_visits[1].resize(FastBoard::MAXSQ);
    
    m_totalwins[0] = 0;
    m_totalwins[1] = 0;
    m_totalvisits[0] = 0;
    m_totalvisits[1] = 0;
}

void AMAFTable::visit(int color, int vtx, bool win) {  
    SMP::Lock lock(m_mutex);
    
    m_wins[color][vtx] += win;
    m_visits[color][vtx]++;
    
    m_totalwins[color] += win;
    m_totalvisits[color]++;   
}

float AMAFTable::get_score(int color, int vertex) {    
    assert(vertex >= 0 && vertex < FastBoard::MAXSQ);
        
    float score = (float)m_wins[color][vertex] / (float)m_visits[color][vertex];
    
    return score;    
}

void AMAFTable::clear() {     
    std::fill(get_AMAFT()->m_wins[0].begin(), get_AMAFT()->m_wins[0].end(), 0);    
    std::fill(get_AMAFT()->m_wins[1].begin(), get_AMAFT()->m_wins[1].end(), 0);    
    std::fill(get_AMAFT()->m_visits[0].begin(), get_AMAFT()->m_visits[0].end(), 0);    
    std::fill(get_AMAFT()->m_visits[1].begin(), get_AMAFT()->m_visits[1].end(), 0);    
        
    get_AMAFT()->m_totalwins[0] = 0;
    get_AMAFT()->m_totalwins[1] = 0;
    get_AMAFT()->m_totalvisits[0] = 0;
    get_AMAFT()->m_totalvisits[1] = 0;
}

bool AMAFTable::is_primed() {
    return (get_AMAFT()->m_totalvisits[0] > 32);
}

float AMAFTable::get_average(int color) {
    float score = (float)m_totalwins[color] / (float)m_totalvisits[color];
    
    return score;    
}