#ifndef ATTRIBUTES_H_INCLUDED
#define ATTRIBUTES_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>

#include "KoState.h"

// Move attributes
class Attributes {
public:           
    Attributes();
    void get_from_move(KoState * state, int move);    
    int get_pattern(void);
private:
    int border_distance(std::pair<int, int> xy, int bsize);
    int move_distance(std::pair<int, int> xy1, std::pair<int, int> xy2);

    //std::vector<bool> m_present; 
    int m_pattern;
};

#endif