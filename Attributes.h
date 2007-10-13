#ifndef ATTRIBUTES_H_INCLUDED
#define ATTRIBUTES_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <bitset>

#include "KoState.h"

// Move attributes
class Attributes {
public:           
    Attributes();
    void get_from_move(FastState * state, int move,  std::vector<int> & mcow);    
    int get_pattern(void);
    bool attribute_enabled(int idx);
private:
    int border_distance(std::pair<int, int> xy, int bsize);
    int move_distance(std::pair<int, int> xy1, std::pair<int, int> xy2);
    
    std::bitset<62> m_present; 
    int m_pattern;
};

#endif