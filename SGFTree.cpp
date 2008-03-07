#include <iostream>
#include <fstream>
#include <cctype>
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "SGFTree.h"
#include "KoState.h"
#include "SGFParser.h"
#include "Utils.h"

using namespace Utils;

SGFTree::SGFTree(void) {    
}

KoState * SGFTree::get_state(void) {
    return &m_state;
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
    helper = *(get_state());   
    
    // game history is not correctly initialized by above
    // so reanchor
    result.anchor_game_history(); 
    
    int tomove = result.get_to_move();
    
    for (unsigned int i = 0; i <= movenum && link != NULL; i++) {
        // root position has no associated move
        if (i != 0) {    
            int move = link->get_move(tomove);
            if (move != SGFTree::EOT) {        
                result.play_move(move);
                tomove = !tomove;
            }
        }
                
        link = link->get_child(0);
    }
    
    return result;
}

KoState * SGFTree::get_state_from_mainline(unsigned int movenum) {     
    SGFTree * link = this;   
    SGFTree * last = this;          

    for (unsigned int i = 0; i <= movenum && link != NULL; i++) {        
        link = link->get_child(0);
        if (link == NULL) {
            return last->get_state();
        } else {
            last = link;
        }
    }

    return link->get_state();
}

// the number of states is one more than the number of moves
int SGFTree::count_mainline_moves(void) {
    SGFTree * link = this;             
    int count = -1;

    while (link != NULL) {        
        link = link->get_child(0);
        count++;
    }

    return count;
}

void SGFTree::load_from_string(std::string gamebuff) {
    std::istringstream pstream(gamebuff);

    // initialize root state with defaults
    m_state.init_game();

    // loads properties with moves
    SGFParser::parse(pstream, this);

    // populates the states from the moves
    // split this up in root node, achor (handicap), other nodes
    populate_states();
}

// load a single game from a file
void SGFTree::load_from_file(std::string filename, int index) {           
    std::string gamebuff = SGFParser::chop_from_file(filename, index); 
    
    //myprintf("Parsing: %s\n", gamebuff.c_str());

    load_from_string(gamebuff);
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
        if (bsize <= FastBoard::MAXBOARDSIZE) {
            m_state.init_game(bsize);
        } else {
            throw std::exception("Board size not supported.");
        }
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

    // handicap stone    
    std::pair<PropertyMap::iterator, 
              PropertyMap::iterator> abrange = m_properties.equal_range("AB");
    for (it = abrange.first; it != abrange.second; ++it) {
        std::string move = it->second;      
        int vtx = string_to_vertex(move);
        apply_move(FastBoard::BLACK, vtx);                
    }            
    
    std::pair<PropertyMap::iterator, 
              PropertyMap::iterator> awrange = m_properties.equal_range("AW");
    for (it = awrange.first; it != awrange.second; ++it) {
        std::string move = it->second;      
        int vtx = string_to_vertex(move);
        apply_move(FastBoard::WHITE, vtx);                
    }                 
    
    it = m_properties.find("PL");
    if (it != m_properties.end()) {
        std::string who = it->second;
        if (who == "W") {
            m_state.set_to_move(FastBoard::WHITE);
        } else if (who == "B") {
            m_state.set_to_move(FastBoard::BLACK);
        }        
    }     
    
    // now for all children play out the moves
    std::vector<SGFTree>::iterator stateit;
    
    for (stateit = m_children.begin(); stateit != m_children.end(); ++stateit) {
        // propagate state    
        stateit->set_state(m_state);
        
        // XXX: maybe move this to the recursive call
        // get move for side to move        
        int move = stateit->get_move(m_state.get_to_move());  
        
        if (move != EOT) {            
            stateit->apply_move(move);                        
        }                                     
        stateit->populate_states();
    }        
}

void SGFTree::set_state(KoState & state) {
    m_state = state;
}


void SGFTree::apply_move(int color, int move) {      
    if (m_state.board.get_square(move) != FastBoard::EMPTY 
        && move != FastBoard::PASS) {
            throw new std::exception("Illegal move");
    }
    m_state.play_move(color, move);    
}

void SGFTree::apply_move(int move) {  
    int color = m_state.get_to_move();
    apply_move(color, move);    
}

void SGFTree::add_property(std::string property, std::string value) {
    m_properties.insert(std::make_pair(property, value));
}

SGFTree * SGFTree::add_child(SGFTree child) {
    // first allocation is better small
    if (m_children.size() == 0) {
        m_children.reserve(1);
    }    
    m_children.push_back(child);
    return &(m_children.back());
}

int SGFTree::string_to_vertex(std::string movestring) {
    if (movestring.size() == 0) {
        return FastBoard::PASS;
    } 
    
    if (m_state.board.get_boardsize() <= 19) {
        if (movestring == "tt") {
            return FastBoard::PASS;
        }
    }
    
    int bsize = m_state.board.get_boardsize();
    
    char c1 = movestring[0];
    char c2 = movestring[1];
    
    int cc1 = c1 - 'a';
    int cc2 = bsize - (c2 - 'a') - 1;
    
    // catch illegal SGF
    if (cc1 < 0 || cc1 >= bsize
        || cc2 < 0 || cc2 >= bsize) {
        throw std::exception("Illegal SGF move");
    }
    
    int vtx = m_state.board.get_vertex(cc1, cc2);
    
    return vtx;
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
        return string_to_vertex(movestring);
    }
    
    return SGFTree::EOT;
}

std::string SGFTree::state_to_string(GameState * pstate, int compcolor) {            
    std::auto_ptr<GameState> state(new GameState);
    
    // make a working copy
    *state = *pstate;
    
    std::string res;
    
    float komi = state->get_komi();
    int timecontrol = state->get_timecontrol()->get_maintime();
    int size = state->board.get_boardsize();
    
    res.append("(;GM[1]FF[4]RU[Chinese]");
    res.append("SZ[" + boost::lexical_cast<std::string>(size) + "]");
    res.append("KM[" + boost::lexical_cast<std::string>(komi) + "]");
    if (compcolor == FastBoard::WHITE) {
        res.append("PW[Leela " + std::string(VERSION) + "]");
        res.append("PB[Human]");
    } else {
        res.append("PB[Leela " + std::string(VERSION) + "]");
        res.append("PW[Human]");
    }
    
    float score = state->final_score();
    
    if (score > 0.0f) {
        res.append("RE[B+" + boost::lexical_cast<std::string>(score) + "]");
    } else {
        res.append("RE[W+" + boost::lexical_cast<std::string>(-score) + "]");
    }
    
    state->rewind();
    
    // check handicap here (anchor point)
    int handicap = 0;
    std::string handicapstr;
    
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int vertex = state->board.get_vertex(i, j);
            int square = state->board.get_square(vertex);
            
            if (square == FastBoard::BLACK) {
                handicap++;
                handicapstr.append("[" + state->board.move_to_text_sgf(vertex) + "]");
            }
        }
    }
    
    if (handicap > 0) {
        res.append("HA[" + boost::lexical_cast<std::string>(handicap) + "]");
        res.append("AB" + handicapstr);
    }    
    
    res.append("\n");
    
    state->forward_move();
    
    int counter = 0;
    do {
        int move = state->get_last_move();
        std::string movestr = state->board.move_to_text_sgf(move);
        if (state->get_to_move() == FastBoard::BLACK) {
            res.append(";W[" + movestr + "]");
        } else {
            res.append(";B[" + movestr + "]");
        }                
        if (++counter % 10 == 0) {
            res.append("\n");
        }
    } while (state->forward_move());
    
    res.append(")");
    
    return res;
}