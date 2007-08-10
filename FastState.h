#ifndef FASTSTATE_H_INCLUDED
#define FASTSTATE_H_INCLUDED

#include "FullBoard.h"

class FastState {
    public:        
        void init_game(int size = 19, float komi = 7.5f);
        int play_random_move();
        int play_random_move(int color);
        int play_pass_fast();
        int play_move_fast(int vertex);
        void set_komi(float komi);     
        int get_passes();
        void set_passes(int val);
        void increment_passes();    

        int get_movenum();
        float calculate_score();
        int estimate_score();
        int get_last_move();
        void reset_game();
        void reset_board();  
        void display_state();
        void move_to_text(int move, char *vertex); 

        FullBoard board;
         
        float m_komi;
        int m_passes;  
        int komove;   
        int movenum;              
        int lastmove;        
};

#endif