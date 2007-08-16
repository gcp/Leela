#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "config.h"

#include "FastState.h"
#include "GameState.h"
#include "FullBoard.h"
#include "Zobrist.h"
#include "Random.h"
#include "Utils.h"

void GameState::init_game(int size, float komi) {
    
    FastState::init_game(size, komi);
    
    hash_history.clear();
    game_history.clear();
    ko_hash_history.clear();
    
    hash_history.push_back(board.calc_hash());
    ko_hash_history.push_back(board.calc_ko_hash());
    
    game_history.push_back(board); 
        
    return;
};

void GameState::reset_game() {
    FastState::reset_game();
    
    hash_history.clear();
    ko_hash_history.clear();
    game_history.clear();
}

int GameState::gen_moves(int *moves) {    
    int num_moves = 0;        
    int color = board.m_tomove;
    
    for (int i = 0; i < board.get_boardsize(); i++) {   
        for (int j = 0; j < board.get_boardsize(); j++) {      
            if (board.get_square(i, j) == FastBoard::EMPTY         
                && board.no_eye_fill(board.get_vertex(i, j))
                && !board.is_suicide(board.get_vertex(i, j), color)
                && board.get_vertex(i, j) != komove) {    
                moves[num_moves++] = i;
            }
        }            
    }  

    /* pass */
    moves[num_moves++] = -1;
    return num_moves;
}

bool GameState::superko(void) {        
    std::vector<uint64>::const_reverse_iterator first = ko_hash_history.rbegin();
    std::vector<uint64>::const_reverse_iterator last = ko_hash_history.rend();  
    std::vector<uint64>::const_reverse_iterator res;
  
    res = std::find(++first, last, board.ko_hash);

    return (res != last);        
}

int GameState::undo_move(void) {
    if (movenum > 0) {
        movenum--;                     
        game_history.pop_back();
        board = game_history.back();
        return 1;
    } else {
        return 0;
    }
}

void GameState::play_pass(void) {
    movenum++;
        
    lastmove = -1;
        
    board.hash  ^= 0xABCDABCDABCDABCDUI64;    
    board.m_tomove = !board.m_tomove;     
        
    board.hash ^= Zobrist::zobrist_pass[get_passes()];
    increment_passes();
    board.hash ^= Zobrist::zobrist_pass[get_passes()];      
    
    hash_history.push_back(board.hash); 
    ko_hash_history.push_back(board.ko_hash);  
    
    game_history.push_back(board);
}

void GameState::play_move(int vertex) {
    play_move(board.m_tomove, vertex);
}

void GameState::play_move(int color, int vertex) {
    if (vertex != -1) {                   
        int kosq = board.update_board(color, vertex);
    
        komove = kosq;   
        lastmove = vertex;
    
        movenum++;
        
        if (board.m_tomove == color) {
            board.hash  ^= 0xABCDABCDABCDABCDUI64;
        }            
        board.m_tomove = !color;        
        
        if (get_passes() > 0) {
            board.hash ^= Zobrist::zobrist_pass[get_passes()];
            set_passes(0);
            board.hash ^= Zobrist::zobrist_pass[0];
        }
    
        hash_history.push_back(board.hash); 
        ko_hash_history.push_back(board.ko_hash);         
    
        game_history.push_back(board);        
    } else {
        play_pass();
    }    
}

int GameState::play_textmove(char *color, char *vertex) {
    int i;
    int who;
    int column, row;
    int boardsize = board.get_boardsize();
    
    /* case conversion */
    i = 0;
    while (color[i] != '\0') {
        color[i] = tolower(color[i]);
        i++;
    }
    
    i = 0;
    while (vertex[i] != '\0') {
        vertex[i] = tolower(vertex[i]);
        i++;
    }
    
    if (!strncmp(color, "w", 1)) {
        who = FullBoard::WHITE;
    } else if (!strncmp(color, "b", 1)) {
        who = FullBoard::BLACK;
    } else return 0;
    
    if (!isalpha(vertex[0])) return 0;
    if (!isdigit(vertex[1])) return 0;    
    if (vertex[0] == 'i') return 0;
        
    if (vertex[0] < 'i') 
        column = vertex[0] - 'a';
    else
        column = (vertex[0] - 'a')-1;
        
    row = atol(&vertex[1]);
    
    if (row > boardsize) return 0;
    if (column >= boardsize) return 0; 
    
    int move = board.get_vertex(column, row - 1);       
        
    play_move(who, move);                           
                   
    return 1;
}

void GameState::stop_clock(int color) {
    timecontrol.stop(color);
}

void GameState::start_clock(int color) {
    timecontrol.start(color);
}

void GameState::display_state() {
    FastState::display_state();

    timecontrol.display_times();
}