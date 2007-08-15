#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <memory>
#include "config.h"
#include "Zobrist.h"
#include "GTP.h"
#include "Random.h"
#include "Utils.h"

using namespace Utils;

int main (int argc, char *argv[]) {        
    int done = false;
    int gtp_mode;
    char input[STR_BUFF];      
    
    /* default to prompt */
    gtp_mode = false;
    
    if (argc > 1) {
        gtp_mode = true;
    }   

    setbuf (stdout, NULL);
    setbuf (stdin, NULL);
    setbuf (stderr, NULL);           
                    
    std::auto_ptr<Random> rng(new Random(5489UL));          
    Zobrist::init_zobrist(*rng);
    
    std::auto_ptr<GameState> maingame(new GameState);    
        
    /* set board limits */    
    float komi = 7.5;         
    maingame->init_game(9, komi);
            
    while (!done) {
        if (!gtp_mode) {
            maingame->display_state();
            myprintf("Ka: ");
        }            
        
        /* get input line */         
        fgets(input, STR_BUFF, stdin);                
        
        /* eat CR/LF */
        input[strlen(input)-1] = '\0';
        
        if (!GTP::execute(*maingame, input)) {
            myprintf("? unknown command\n");                   
        } 
    }    
    return 0;
}

