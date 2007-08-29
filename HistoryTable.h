#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include <vector>

class HistoryEntry {
public:
    HistoryEntry();

    int m_wins;
    int m_visits;
};

class HistoryTable {
public:        
    /*
        return the global TT
    */            
    static HistoryTable * get_HT();
    static void clear();    
    
    /*
        update corresponding entry
    */            
    void update(int vertex, bool win);     

    float get_score(int vertex);
    int get_visits(int vertex);
    
private:   
    HistoryTable();

    std::vector<HistoryEntry> m_entries;

    static HistoryTable* s_htable;       
};

#endif
