#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "config.h"

#include "KoState.h"
#include "GameState.h"
#include "FullBoard.h"
#include "Zobrist.h"
#include "Random.h"
#include "Utils.h"

void GameState::init_game(int size, float komi) {        
    
    KoState::init_game(size, komi);
        
    game_history.clear();        
    game_history.push_back(board); 
    
    TimeControl tmp(size);
    m_timecontrol = tmp;
        
    return;
};

void GameState::reset_game() {
    KoState::reset_game();
      
    game_history.clear();
    
    TimeControl tmp(board.get_boardsize());
    m_timecontrol = tmp;
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

int GameState::undo_move(void) {
    if (movenum > 0) {
        // This also restores hashes as they're part of
        // the game state
        movenum--;                     
        game_history.pop_back();        
        board = game_history.back();
        return 1;
    } else {
        return 0;
    }
}

void GameState::play_pass(void) {
    KoState::play_pass();
               
    game_history.push_back(board);
}

void GameState::play_move(int vertex) {
    play_move(board.m_tomove, vertex);
}

void GameState::play_move(int color, int vertex) {
    if (vertex != FastBoard::PASS) {                   
        KoState::play_move(color, vertex);        

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
    m_timecontrol.stop(color);
}

void GameState::start_clock(int color) {
    m_timecontrol.start(color);
}

void GameState::display_state() {
    FastState::display_state();

    m_timecontrol.display_times();
}

TimeControl * GameState::get_timecontrol() {
    return &m_timecontrol;
}

void GameState::set_timecontrol(int maintime, int byotime, int byostones) {
    TimeControl timecontrol(board.get_boardsize(), maintime, byotime, byostones);
    
    m_timecontrol = timecontrol;
}

void GameState::adjust_time(int color, int time, int stones) {
    m_timecontrol.adjust_time(color, time, stones);
}