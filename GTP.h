#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include <string>
#include "GameState.h"

class GTP {
public:                        
    static int execute(GameState &game, char *xinput);
private:
    static std::string get_life_list(GameState & game, bool live);    
    static const std::string s_commands[];
};


#endif