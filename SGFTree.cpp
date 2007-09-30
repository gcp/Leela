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

SGFTree * SGFTree::get_child(unsigned int count) {
    if (count < m_children.size()) {
        return &(m_children[count]);
    } else {
        return NULL;
    }        
}

// this follows the entire line, and doesn't really
// need the intermediate states
GameState SGFTree::get_mainline(unsigned int movenum) {
    GameState result;        
    SGFTree * link = this;
        
    KoState & helper = result;
    helper = get_state();    
    
    int tomove = result.get_to_move();
    
    for (unsigned int i = 0; i <= movenum && link != NULL; i++) {
        // root position has no associated move
        if (i != 0) {    
            int move = link->get_move(tomove);        
            result.play_move(move);
            tomove = !tomove;
        }
                
        link = link->get_child(0);
    }
    
    return result;
}

// load a single game from a file
void SGFTree::load_from_file(std::string filename) {           
    std::string gamebuff = SGFParser::chop_from_file(filename, 1);        
    std::istringstream pstream(gamebuff);
    
    // initialize root state with defaults
    m_state.init_game();
    
    // loads properties with moves
    SGFParser::parse(pstream, this);
    
    // populates the states from the moves
    populate_states();
}

void SGFTree::populate_states(void) {
    PropertyMap::iterator it;
    
    // first check for go game setup in properties    
    it = m_properties.find("GM");
    if (it != m_properties.end()) {
        if (it->second != "1") {
            throw new std::exception("SGF Game is not a Go game");
        }        
    }
    
    // board size
    it = m_properties.find("SZ");
    if (it != m_properties.end()) {
        std::string size = it->second;
        std::istringstream strm(size);
        int bsize;
        strm >> bsize;
        m_state.init_game(bsize);
    }
    
    // komi
    it = m_properties.find("KM");
    if (it != m_properties.end()) {
        std::string size = it->second;
        std::istringstream strm(size);
        float komi;
        strm >> komi;
        m_state.init_game(m_state.board.get_boardsize(), komi);
    }
    
    
    // now for all children play out the moves
    std::vector<SGFTree>::iterator stateit;
    
    for (stateit = m_children.begin(); stateit != m_children.end(); ++stateit) {
        // propagate state    
        stateit->set_state(m_state);
        
        // get move for side to move        
        int move = stateit->get_move(m_state.get_to_move());  
        
        if (move != EOT) {            
            stateit->apply_move(move);            
            stateit->populate_states();
        }                                  
    }        
}

void SGFTree::set_state(KoState & state) {
    m_state = state;
}

void SGFTree::apply_move(int move) {    
    m_state.play_move(move);    
}

void SGFTree::add_property(std::string property, std::string value) {
    m_properties[property] = value;
}

SGFTree * SGFTree::add_child(SGFTree child) {
    m_children.push_back(child);
    return &(m_children.back());
}

int SGFTree::get_move(int tomove) {
    std::string movestring;
    
    if (tomove == FastBoard::BLACK) {
        movestring = "B";
    } else {
        movestring = "W";
    }
    
    PropertyMap::iterator it;        
    it = m_properties.find(movestring);
    
    if (it != m_properties.end()) {
        std::string movestring = it->second;
        
        if (movestring == "") {
            return FastBoard::PASS;
        }
        
        int bsize = m_state.board.get_boardsize();
        
        char c1 = movestring[0];
        char c2 = movestring[1];
        
        int cc1 = c1 - 'a';
        int cc2 = bsize - (c2 - 'a') - 1;
        
        int vtx = m_state.board.get_vertex(cc1, cc2);
        
        return vtx;
    }
    
    return SGFTree::EOT;
}
