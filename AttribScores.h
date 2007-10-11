#ifndef ATTRIBSCORES_H_INCLUDED
#define ATTRIBSCORES_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <list>

#include "Attributes.h"

// Move attributes
class AttribScores {
public:          
    void autotune_from_file(std::string filename);    
    void load_from_file(std::string filename);
    
    static AttribScores* get_attribscores(void);

    std::vector<float> m_pweight;  
    std::vector<float> m_fweight;

private:        
    typedef std::vector<Attributes> AttrList;
    typedef std::pair<int, AttrList> LrnPos;
    typedef std::list<LrnPos> LearnVector;

    void gather_attributes(std::string filename, LearnVector & data);
    float team_strength(Attributes & team);
            
    static AttribScores* s_attribscores; 

      
};

#endif