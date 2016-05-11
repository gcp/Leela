#ifndef FASTSTATE_H_INCLUDED
#define FASTSTATE_H_INCLUDED

#include <vector>

#include "FullBoard.h"

class FastState {
public:        
    void init_game(int size = FastBoard::MAXBOARDSIZE, float komi = 7.5f);
    void reset_game();
    void reset_board();  
            
    int play_random_move();
    int play_random_move(int color);    
    int play_move_fast(int vertex);
    float score_move(std::vector<int> & territory, std::vector<int> & moyo, int vertex);
    
    void play_pass(void);
    void play_move(int vertex);

    std::vector<int> generate_moves(int color);
    bool try_move(int color, int vertex, bool allow_sa = false);

    void set_komi(float komi);        
    float get_komi(); 
    void set_handicap(int hcap);        
    int get_handicap();
    int get_passes();
    int get_to_move();
    void set_to_move(int tomove);
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
    int get_komove();
    void display_state();    
    std::string move_to_text(int move);

    FullBoard board;

    float m_komi;
    int m_handicap;
    int m_passes;
    int m_komove;
    size_t m_movenum;
    int m_lastmove;
    int m_onebutlastmove;

protected:
    FastBoard::movelist_t moves;
    FastBoard::scoredlist_t scoredmoves;

    int walk_empty_list(int color, int vidx, bool allow_sa = false);
    void play_move(int color, int vertex);
};

#endif
