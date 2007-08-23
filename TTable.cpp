#include "config.h"

#include "TTable.h"
#include "TTEntry.h"

TTable* TTable::s_ttable = 0;

TTable* TTable::get_TT(void) {
    if (s_ttable == 0) {
        s_ttable = new TTable;
    }
    
    return s_ttable;
}

TTable::TTable(int size) {
    m_buckets.resize(size);
}