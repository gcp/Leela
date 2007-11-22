#ifndef FASTSTATE_H_INCLUDED
#define FASTSTATE_H_INCLUDED

#include <vector>

#include "FullBoard.h"

class FastState {
public:        
    void init_game(int size = 19, float komi = 7.5f);
    void reset_game();
    void reset_board();  
        
    int play_random_move_sa(int color);    
    int play_random_move();
    int play_random_move(int color);    
    int play_move_fast(int vertex);
    float score_move(std::vector<int> & territory, std::vector<int> & moyo, int vertex);
    
    void play_pass(void);    
    void play_move(int vertex); 
    
    std::vector<int> generate_moves(int color);
          
    void set_komi(float komi);        
    float get_komi(); 
    int get_passes();
    int get_to_move();
    void set_passes(int val);
    void increment_passes();            
    
    float calculate_mc_score();
    int estimate_mc_score();
    float final_score();
    std::vector<int> final_score_map();
    std::vector<bool> mark_dead();
    
    int get_movenum();
    int get_last_move();  
    int get_prevlast_move();      
    void display_state();    
    std::string move_to_text(int move);

    FullBoard board;
     
    float m_komi;
    int m_passes;
    int komove;   
    int movenum;              
    int lastmove;   
    int onebutlastmove;     
    
protected:
    std::vector<int> m_work;   // working area for move generation

    int walk_empty_list(int color, int vidx, bool allow_sa = false);
    bool try_move(int color, int vertex, bool allow_sa = false);
    void play_move(int color, int vertex);     
};

#endif
