#ifndef ATTRIBSCORES_H_INCLUDED
#define ATTRIBSCORES_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>

#include "Attributes.h"

// Move attributes
class AttribScores {
public:          
    void autotune_from_file(std::string filename);                 
private:    
    typedef std::pair<int, Attributes> MoveAttrPair;
    typedef std::vector<MoveAttrPair> MoveAttrList;

    std::vector<float> m_weight;    
};

#endif