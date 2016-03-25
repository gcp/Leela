#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#ifdef USE_NETS
#include <glog/logging.h>
#endif

#include "Zobrist.h"
#include "GTP.h"
#include "Random.h"
#include "Utils.h"
#include "Matcher.h"
#include "AttribScores.h"

using namespace Utils;

#ifdef _CONSOLE
int main (int argc, char *argv[]) {        
    int done = false;
    int gtp_mode;
    std::string input;

#ifdef USE_NETS
    ::google::InitGoogleLogging(argv[0]);
#endif

    /* default to prompt */
    gtp_mode = false;
    
    if (argc > 1) {
        gtp_mode = true;
    }   

    std::cout.setf(std::ios::unitbuf);
    std::cin.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);    
    
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    setbuf(stderr, NULL);    
                    
    std::unique_ptr<Random> rng(new Random(5489UL));
    Zobrist::init_zobrist(*rng);    

    AttribScores::get_attribscores();
    Matcher::get_Matcher();
    
    std::unique_ptr<GameState> maingame(new GameState);
        
    /* set board limits */    
    float komi = 7.5;         
    maingame->init_game(9, komi);
            
    while (!done) {
        if (!gtp_mode) {
            maingame->display_state();
            myprintf("Leela: ");
        }            
                
        std::getline(std::cin, input);               
        GTP::execute(*maingame, input);            
    }    
    return 0;
}
#endif
