#ifndef FULLBOARD_H_INCLUDED
#define FULLBOARD_H_INCLUDED

#include "config.h"
#include "FastBoard.h"

class FullBoard : public FastBoard {
public:
    int remove_string(int i);
    int update_board(const int color, const int i);

    uint64 calc_hash(void);
    uint64 calc_ko_hash(void);
    uint64 get_hash(void);
    uint64 get_ko_hash(void);
    std::vector<uint64> get_rotated_hashes(void);

    // calculates hash after move without executing it
    // good for calculating superko
    uint64 predict_ko_hash(int color, int move);

    void reset_board(int size); 
    void display_board(int lastmove = -1);
        
    uint64 hash;   
    uint64 ko_hash;
};

#endif
