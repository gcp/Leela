#ifndef FULLBOARD_H_INCLUDED
#define FULLBOARD_H_INCLUDED

#include "config.h"
#include "FastBoard.h"

class FullBoard : public FastBoard {
    public:                       
        int remove_string(int i);        
        int update_board(const int color, const int i);
        
        uint64 calc_hash(void);
        uint64 get_hash(void);
        uint64 calc_ko_hash(void);
        
        void reset_board(int size); 
        void display_board(int lastmove = -1);
            
        uint64 hash;   
        uint64 ko_hash;
};

#endif