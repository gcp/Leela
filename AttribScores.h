#ifndef ATTRIBSCORES_H_INCLUDED
#define ATTRIBSCORES_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <list>
#include <set>
#include <map>

#include "Attributes.h"

// Move attributes
class AttribScores {
public:          
    void autotune_from_file(std::string filename);        
    float team_strength(Attributes & team);
    
    static AttribScores* get_attribscores(void);
      
    std::vector<float> m_fweight;
    std::map<uint64, float> m_pat;

private:        
    typedef std::vector<Attributes> AttrList;
    typedef std::pair<size_t, AttrList> LrnPos;
    typedef std::list<LrnPos> LearnVector;

    void gather_attributes(std::string filename, LearnVector & data);
    float get_patweight(uint64 idx);
    void set_patweight(uint64 idx, float val);
    void load_internal();
                
    static AttribScores* s_attribscores;       
};

#endif
