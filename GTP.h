#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include "GameState.h"

class GTP {
    public:                        
        static int execute(GameState &game, char *xinput);
};


#endif