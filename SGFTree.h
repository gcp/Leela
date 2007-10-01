#ifndef SGFTREE_H_INCLUDED
#define SGFTREE_H_INCLUDED

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include "KoState.h"
#include "GameState.h"

class SGFTree {
public:
    SGFTree();

    KoState get_state();
    KoState get_state_from_mainline(unsigned int count);
    GameState get_mainline(unsigned int movenum = 999);
    void load_from_file(std::string filename); 
    
    int count_mainline_moves(void);

    void add_property(std::string property, std::string value); 
    SGFTree * add_child(SGFTree child);                  

private:     
    static const int EOT = 0;               // End-Of-Tree marker

    void populate_states(void);
    int get_move(int tomove);
    void apply_move(int move);
    void set_state(KoState & state);
    SGFTree * get_child(unsigned int count);

    typedef std::map<std::string, std::string> PropertyMap;

    KoState m_state;    
    std::vector<SGFTree> m_children;
    PropertyMap m_properties;
};

#endif