#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "config.h"
#include "Utils.h"
#include "GameState.h"
#include "GTP.h"
#include "Playout.h"
#include "UCTSearch.h"

using namespace Utils;

int GTP::execute(GameState& game, char *xinput) {
    char input[STR_BUFF];
    char command[STR_BUFF], command2[STR_BUFF];
    char color[STR_BUFF], vertex[STR_BUFF];
    int id = -1;
    int tmp, nw;
    float ftmp;
    /* parse */    
    
    /* eat empty lines, simple preprocessing */
    for (tmp = 0, nw = 0; xinput[tmp] != '\0'; tmp++) {
        if (xinput[tmp] == 9) {
            input[nw++] = 32;
        } else if ((xinput[tmp] > 0 && xinput[tmp] <= 9)
	        || (xinput[tmp] >= 11 && xinput[tmp] <= 31)
	        || xinput[tmp] == 127) {
	       continue;
        } else {        
            input[nw++] = tolower(xinput[tmp]);
        }            
        
        /* eat multi whitespace */
        if (nw >= 2 && input[nw-2] == 32 && input[nw-1] == 32) {
            nw--;
        }
    }    
    input[nw] = '\0';
    
    if (!strcmp(input, "")) {
        return 1;
    } else if (!strcmp(input, "exit")) {
        exit(EXIT_SUCCESS);
        return 1;
    } else if (!strncmp(input, "#", 1)) {
        return 1;
    } else if (isdigit(input[0])) {
        /* starts with number, likely GTP command */
        sscanf(input, "%d %[^\n\r]s", &id, command);                        
    } else {
        strncpy(command, input, STR_BUFF);
    }
    
    /* process commands */
    if (!strcmp(command, "protocol_version")) {
        gtp_printf(id, GTP_VERSION);
        return 1;
    } else if (!strcmp (command, "name")) {
        gtp_printf(id, NAME);
        return 1;  
    } else if (!strcmp (command, "version")) {
        gtp_printf(id, VERSION);
        return 1;
    } else if (!strcmp (command, "quit")) {
        gtp_printf(id, "");
        exit(EXIT_SUCCESS);        
    } else if (!strncmp (command, "known_command", 13)) {
        strncpy(command2, command+14, STR_BUFF);
        
        if (!strcmp(command2, "protocol_version")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "name")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "version")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "quit")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "boardsize")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "clear_board")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "komi")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "play")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "genmove")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp(command2, "known_command")) {
            gtp_printf(id, "true");
            return 1;
        } else if (!strcmp (command2, "undo")) {
            gtp_printf(id, "true");
            return 1;    
        } else if (!strcmp (command2, "final_score")) {
            gtp_printf(id, "true");
            return 1;        
        } else if (!strcmp (command2, "showboard")) {
            gtp_printf(id, "true");
            return 1;        
        } else {
            gtp_printf(id, "false");
        }
        return 1;
    } else if (!strcmp (command, "list_commands")) {
        gtp_printf(id, "protocol_version\nname\nversion\nquit\nknown_command\nlist_commands\n"
                       "quit\nboardsize\nclear_board\nkomi\nplay\ngenmove\nshowboard\nundo\nfinal_score");
        return 1;
    } else if (!strncmp (command, "boardsize", 9)) {
        if (sscanf(command+10, "%d", &tmp) == 1) {
            if (tmp < 2 || tmp > FastBoard::MAXBOARDSIZE) {
                gtp_fail_printf(id, "unacceptable size");
            } else {
                game.init_game(tmp);                
                gtp_printf(id, "");
            }
        } else {
            gtp_fail_printf(id, "syntax error");
        }
        return 1;
    } else if (!strcmp (command, "clear_board")) {
        game.reset_board();
        gtp_printf(id, "");
        return 1;
    } else if (!strncmp (command, "komi", 4)) {
        float komi;
        if (sscanf(command+5, "%f", &komi) != 1) {
            gtp_fail_printf(id, "syntax error");
         } else {
            game.set_komi(komi);
            gtp_printf(id, "");
         }
        return 1;
    } else if (!strncmp (command, "play", 4)) {
        if (strstr(command+5, "pass")) {
            game.play_pass();
            gtp_printf(id, "");
        } else if (sscanf(command+5, "%s %s", color, vertex) == 2) {            
            if (!game.play_textmove(color, vertex)) {
                gtp_fail_printf(id, "illegal move");
            } else {
                gtp_printf(id, "");
            }
        } else {
            gtp_fail_printf(id, "syntax error");
        };
        return 1;
    } else if (!strncmp (command, "genmove", 7 )) {
        int who;
        if (!strcmp(command+8, "w") || !strcmp(command+8, "white")) {
            who = FastBoard::WHITE;            
        } else if (!strcmp(command+8, "b") || !strcmp(command+8, "black")) {
            who = FastBoard::BLACK;         
        } else {
            gtp_fail_printf(id, "syntax error");
            return 1;
        }   

        std::auto_ptr<UCTSearch> search(new UCTSearch(game));

        int move = search->think(who);
        game.play_move(who, move);                    

        game.move_to_text(move, vertex);            
        gtp_printf(id, "%s", vertex);
                
        return 1;
    } else if (!strcmp (command, "undo")) {
        if (game.undo_move()) {
            gtp_printf(id, "");
        } else {
            gtp_fail_printf(id, "cannot undo");
        }            
        return 1;
    } else if (!strcmp (command, "showboard")) {
        gtp_printf(id, "");
        game.display_state();
        return 1;
    } else if (!strcmp (command, "final_score")) {
        ftmp = game.board.final_score();   
        /* white wins */        
        if (ftmp < -0.1) {
            gtp_printf(id, "W+%3.1f", (float)fabs(ftmp));
        } else if (ftmp > 0.1) {
            gtp_printf(id, "B+%3.1f", ftmp);
        } else {
            gtp_printf(id, "0");
        }                
        return 1;
    } else if (!strcmp (command, "auto")) {    
        do {
            std::auto_ptr<UCTSearch> search(new UCTSearch(game));

            int move = search->think(game.get_to_move());
            game.play_move(move);  
            game.display_state();
                              
        } while (game.get_passes() < 2);

        return 1;
    } else if (!strcmp (command, "bench")) {
        Playout::do_playout_benchmark(game);
        return 1;
    } else if (!strcmp (command, "influence")) {
        gtp_printf(id, "");
        game.board.display_influence();        
        return 1;
    }
    
    /* unknown command, if GTP give GTP error else let main decide */
    if (id != -1) {
        gtp_fail_printf(id, "unknown command");
        return 1;
    } else {
        return 0;
    }
}

