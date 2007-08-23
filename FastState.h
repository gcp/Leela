#ifndef FASTSTATE_H_INCLUDED
#define FASTSTATE_H_INCLUDED

#include <vector>

#include "FullBoard.h"

class FastState {
public:        
    void init_game(int size = 19, float komi = 7.5f);
    void reset_game();
    void reset_board();  
    
    int play_random_move();
    int play_random_move(int color);
    void play_pass_fast();
    int play_move_fast(int vertex);
    
    void play_pass(void);    
    void play_move(int vertex); 
          
    void set_komi(float komi);         
    int get_passes();
    int get_to_move();
    void set_passes(int val);
    void increment_passes();            
    
    float calculate_mc_score();
    int estimate_score();
    float final_score();
    std::vector<bool> mark_dead();
    
    int get_movenum();
    int get_last_move();        
    void display_state();
    void move_to_text(int move, char *vertex); 

    FullBoard board;
     
    float m_komi;
    int m_passes;  
    int komove;   
    int movenum;              
    int lastmove;        
    
protected:
    void play_move(int color, int vertex);     
};

#endif