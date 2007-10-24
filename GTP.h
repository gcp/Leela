#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include <string>
#include "GameState.h"

class GTP {
public:                        
    static bool execute(GameState &game, std::string xinput);
private:        
    static const int GTP_VERSION = 2;
    
    static std::string get_life_list(GameState & game, bool live);    
    static const std::string s_commands[];
};


#endif
