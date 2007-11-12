#ifndef GAMESTATE_H_INCLUDED
#define GAMESTATE_H_INCLUDED

#include <vector>
#include <string>

#include "FastState.h"
#include "FullBoard.h"
#include "KoState.h"
#include "TimeControl.h"

class GameState : public KoState {
public:                    
    void init_game(int size = 19, float komi = 7.5f);    
    void reset_game();        
    bool set_fixed_handicap(int stones); 
    void place_free_handicap(int stones);
    void anchor_game_history(void);        
    
    bool undo_move(void);
    bool forward_move(void);
    
    void play_pass(void);
    void play_move(int color, int vertex);      
    void play_move(int vertex); 
    bool play_textmove(std::string color, std::string vertex);                            
    
    void start_clock(int color);
    void stop_clock(int color);
    TimeControl * get_timecontrol();
    void set_timecontrol(int maintime, int byotime, int byostones);
    void adjust_time(int color, int time, int stones);

    void display_state();
          
private:
    bool valid_handicap(int stones);        
    
    std::vector<FastState> game_history;                          
    
    TimeControl m_timecontrol;               
};

#endif
