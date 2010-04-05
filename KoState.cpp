#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "config.h"

#include "FastState.h"
#include "FullBoard.h"
#include "KoState.h"

void KoState::init_game(int size, float komi) { 
    assert(size <= FastBoard::MAXBOARDSIZE);
                       
    FastState::init_game(size, komi);
        
    ko_hash_history.clear();
    hash_history.clear();

    ko_hash_history.push_back(board.calc_ko_hash());    
    hash_history.push_back(board.calc_hash());                
}

bool KoState::legal_move(int vertex) {
    if (board.get_square(vertex) != FastBoard::EMPTY) {
        return false;
    }   
    if (board.is_suicide(vertex, board.get_to_move())) {
        return false;
    }
    
    KoState tmp = *this;    
    
    tmp.play_move(vertex);
    
    if (tmp.superko()) {
        return false;
    }
    
    return true;
}

bool KoState::superko(void) {        
    std::vector<uint64>::const_reverse_iterator first = ko_hash_history.rbegin();
    std::vector<uint64>::const_reverse_iterator last = ko_hash_history.rend();  
    std::vector<uint64>::const_reverse_iterator res;
  
    res = std::find(++first, last, board.ko_hash);

    return (res != last);        
}

bool KoState::superko(uint64 newhash) {
    std::vector<uint64>::const_reverse_iterator first = ko_hash_history.rbegin();
    std::vector<uint64>::const_reverse_iterator last = ko_hash_history.rend();  
    std::vector<uint64>::const_reverse_iterator res;
  
    res = std::find(first, last, newhash);

    return (res != last);        
}

void KoState::reset_game() {
    FastState::reset_game();
        
    ko_hash_history.clear();
    hash_history.clear();
    
    ko_hash_history.push_back(board.calc_ko_hash());    
    hash_history.push_back(board.calc_hash());                
}

void KoState::play_pass(void) {
    FastState::play_pass();
        
    ko_hash_history.push_back(board.ko_hash);          
    hash_history.push_back(board.hash);          
}

void KoState::play_move(int vertex) {
    play_move(board.m_tomove, vertex);
}

void KoState::play_move(int color, int vertex) {
    if (vertex != FastBoard::PASS && vertex != FastBoard::RESIGN) {                   
        FastState::play_move(color, vertex);        
            
        ko_hash_history.push_back(board.ko_hash);                 
        hash_history.push_back(board.hash); 
    } else {
        play_pass();
    }    
}
