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
    
    KoState get_state(int movenum);
    KoState get_state();
    void load_from_file(std::string filename); 
    
    void add_property(std::string property, std::string value); 
    SGFTree * add_child(SGFTree child);                  
    
private:     
    static const int EOT = 0;               // End-Of-Tree marker
    
    void populate_states(void);
    int get_move(int tomove);
    void apply_move(int move);
    void set_state(KoState & state);
    
    typedef std::map<std::string, std::string> PropertyMap;
        
    KoState m_state;    
    std::vector<SGFTree> m_children;
    PropertyMap m_properties;
};

#endif