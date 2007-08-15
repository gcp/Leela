#ifndef GAMESTATE_H_INCLUDED
#define GAMESTATE_H_INCLUDED

#include "FastState.h"
#include "FullBoard.h"

#include <vector>

class GameState : public FastState {
    public:                    
        void init_game(int size = 19, float komi = 7.5f);
        bool superko(void);
        void reset_game();        
        
        int gen_moves(int *moves);
        
        int undo_move(void);
        void play_pass(void);
        void play_move(int color, int vertex);      
        void play_move(int vertex); 
        int play_textmove(char *color, char *vertex);                
        
        std::vector<FullBoard> game_history;   
        std::vector<uint64> hash_history;                    
        std::vector<uint64> ko_hash_history;                       
};

#endif