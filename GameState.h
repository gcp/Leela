#ifndef GAMESTATE_H_INCLUDED
#define GAMESTATE_H_INCLUDED

#include <vector>

#include "FastState.h"
#include "FullBoard.h"
#include "KoState.h"
#include "TimeControl.h"

class GameState : public KoState {
public:                    
    void init_game(int size = 19, float komi = 7.5f);    
    void reset_game();        
    
    int gen_moves(int *moves);
    
    int undo_move(void);
    void play_pass(void);
    void play_move(int color, int vertex);      
    void play_move(int vertex); 
    int play_textmove(char *color, char *vertex);                            
    
    void start_clock(int color);
    void stop_clock(int color);
    TimeControl * get_timecontrol();
    void set_timecontrol(int maintime, int byotime, int byostones);
    void adjust_time(int color, int time, int stones);

    void display_state();
          
private:
    std::vector<FullBoard> game_history;                          
    
    TimeControl m_timecontrol;               
};

#endif
