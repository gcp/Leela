#ifndef KOSTATE_H_INCLUDED
#define KOSTATE_H_INCLUDED

#include <vector>

#include "FastState.h"
#include "FullBoard.h"

class KoState : public FastState {
public:                    
    void init_game(int size = 19, float komi = 7.5f);
    bool superko(void);
    void reset_game();
    
    bool legal_move(int vertex);      
        
    void play_pass(void);
    void play_move(int color, int vertex);
    void play_move(int vertex);               
          
private:         
    std::vector<uint64> ko_hash_history;   
    std::vector<uint64> hash_history;     
};

#endif
