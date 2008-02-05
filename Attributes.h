#ifndef ATTRIBUTES_H_INCLUDED
#define ATTRIBUTES_H_INCLUDED

#include "config.h"

#include <vector>
#include <string>
#include <bitset>

#include "KoState.h"

// Move attributes for full move ordering
class Attributes {
public:               
    void get_from_move(FastState * state, 
                       std::vector<int> & territory,
                       std::vector<int> & moyo, 
                       int move);    
    uint64 get_pattern(void);    
    bool attribute_enabled(int idx);    
    static int border_distance(std::pair<int, int> xy, int bsize);
    static int move_distance(std::pair<int, int> xy1, std::pair<int, int> xy2);    
private:        
    int m_pattern;
    std::bitset<103> m_present;     
};

// Move attributes for quick move ordering
class FastAttributes {
public:               
    void get_from_move(FastState * state, 
                       std::vector<int> & territory,
                       std::vector<int> & moyo, 
                       int move);    
    uint64 get_pattern(void);    
    bool attribute_enabled(int idx);    
private:         
    int m_pattern;
    std::bitset<14> m_present; 
};

#endif
