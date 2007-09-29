#include <iostream>
#include <fstream>
#include <cctype>
#include <sstream>

#include "SGFTree.h"
#include "KoState.h"
#include "SGFParser.h"

SGFTree::SGFTree(void) {
}

KoState SGFTree::get_state(void) {
    return m_state;
}

KoState SGFTree::get_move(int movenum) {
    return get_state();
}

// load a single game from a file
void SGFTree::load_from_file(std::string filename) {           
    std::string gamebuff = SGFParser::chop_from_file(filename, 1);        
    std::istringstream pstream(gamebuff);
    
    SGFParser::parse(pstream, this);
}
