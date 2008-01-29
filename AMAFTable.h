#ifndef AMAFTABLE_H_INCLUDED
#define AMAFTABLE_H_INCLUDED

#include <vector>
#include "Playout.h"
#include "SMP.h"

class AMAFTable {
public:            
    static AMAFTable * get_AMAFT();
    static void clear();    
    
    /*
        update corresponding entry
    */            
    void visit(int color, int vertex, bool win);    
    
    float get_score(int color, int vertex);
    bool is_primed();    
    
private:   
    AMAFTable();

    std::tr1::array<std::vector<int>, 2> m_wins;
    std::tr1::array<std::vector<int>, 2> m_visits;
    std::tr1::array<int, 2> m_totalwins;
    std::tr1::array<int, 2> m_totalvisits;
    SMP::Mutex m_mutex;

    static AMAFTable* s_amaftable;       
};

#endif
