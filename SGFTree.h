#ifndef SGFTREE_H_INCLUDED
#define SGFTREE_H_INCLUDED

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include "KoState.h"

class SGFTree {
public:
    SGFTree();
    
    KoState get_move(int movenum);
    KoState get_state();
    void load_from_file(std::string filename);    
    
private:         
    KoState m_state;
    
    std::vector<SGFTree> m_children;
    std::map<std::string, std::string> m_properties;
};

#endif