#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>

#include "config.h"
#include "Utils.h"
#include "GameState.h"
#include "GTP.h"
#include "Playout.h"
#include "UCTSearch.h"

using namespace Utils;

const std::string GTP::s_commands[] = {
    "protocol_version",
    "name",
    "version",
    "quit",
    "known_command",
    "list_commands",
    "quit",
    "boardsize",
    "clear_board",
    "komi",
    "play",
    "genmove",
    "showboard",
    "undo",
    "final_score",
    "final_status_list",
    "time_settings",
    "time_left",
    "kgs-genmove_cleanup",
    ""
};

std::string GTP::get_life_list(GameState & game, bool live) {
    std::vector<std::string> stringlist;
    std::string result;
    FastBoard & board = game.board;
    
    std::vector<bool> dead = game.mark_dead();        
    
    for (int i = 0; i < board.get_boardsize(); i++) {
        for (int j = 0; j < board.get_boardsize(); j++) {
            int vertex = board.get_vertex(i, j);           
            
            if (board.get_square(vertex) != FastBoard::EMPTY) {                                
                if (live ^ dead[vertex]) {
                    stringlist.push_back(board.get_string(vertex));                    
                }                
            }
        }
    }    
    
    // remove multiple mentions of the same string
    // unique reorders and returns new iterator, erase actually deletes
    std::sort(stringlist.begin(), stringlist.end());    
    stringlist.erase(std::unique(stringlist.begin(), stringlist.end()), stringlist.end());
    
    for (int i = 0; i < stringlist.size(); i++) {
        result += (i == 0 ? "" : "\n") + stringlist[i];
    }
    
    return result;                    
}

int GTP::execute(GameState & game, char *xinput) {
    char input[STR_BUFF];
    char command[STR_BUFF];
    char color[STR_BUFF], vertex[STR_BUFF];
    int nw, id = -1;    
    float ftmp;
    
    /* parse */            
    nw = 0;
    /* eat empty lines, simple preprocessing, lower case */
    for (int tmp = 0; xinput[tmp] != '\0'; tmp++) {
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
        std::string cmd(command);
        std::istringstream cmdstream(cmd, std::istringstream::in);
        std::string tmp;
        
        cmdstream >> tmp;     /* remove known_command */
        cmdstream >> tmp;
        
        for (int i = 0; s_commands[i].size() > 0; i++) {
            if (tmp == s_commands[i]) {
                gtp_printf(id, "true");
                return 1;
            }
        }
        
        gtp_printf(id, "false");        
        return 1;
    } else if (!strcmp (command, "list_commands")) {
        std::string outtmp(s_commands[0]);
        for (int i = 1; s_commands[i].size() > 0; i++) {            
            outtmp = outtmp + "\n" + s_commands[i];            
        }
        gtp_printf(id, outtmp.c_str());    
        return 1;
    } else if (!strncmp (command, "boardsize", 9)) {
        int tmp;
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
        game.reset_game();
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
    } else if (!strncmp (command, "kgs-genmove_cleanup", 19 )) {
        int who;
        if (!strcmp(command+20, "w") || !strcmp(command+20, "white")) {
            who = FastBoard::WHITE;            
        } else if (!strcmp(command+20, "b") || !strcmp(command+20, "black")) {
            who = FastBoard::BLACK;         
        } else {
            gtp_fail_printf(id, "syntax error");
            return 1;
        }   

        std::auto_ptr<UCTSearch> search(new UCTSearch(game));

        int move = search->think(who, UCTSearch::NOPASS);
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
    } else if (!strcmp (command, "mc_score")) {
        ftmp = game.board.final_mc_score();   
        /* white wins */        
        if (ftmp < -0.1) {
            gtp_printf(id, "W+%3.1f", (float)fabs(ftmp));
        } else if (ftmp > 0.1) {
            gtp_printf(id, "B+%3.1f", ftmp);
        } else {
            gtp_printf(id, "0");
        }                
        return 1;
    } else if (!strcmp (command, "final_score")) {
        ftmp = game.final_score();   
        /* white wins */        
        if (ftmp < -0.1) {
            gtp_printf(id, "W+%3.1f", (float)fabs(ftmp));
        } else if (ftmp > 0.1) {
            gtp_printf(id, "B+%3.1f", ftmp);
        } else {
            gtp_printf(id, "0");
        }                
        return 1;
    } else if (!strncmp (command, "final_status_list", 17)) {
        if (strstr(command, "alive")) {
            std::string livelist = get_life_list(game, true);
            gtp_printf(id, livelist.c_str());                        
        } else if (strstr(command, "dead")) {            
            std::string deadlist = get_life_list(game, false);
            gtp_printf(id, deadlist.c_str());                        
        } else {
            gtp_printf(id, "");
        }
        return 1;
    } else if (!strncmp (command, "time_settings", 13)) {
        std::string cmdstring(command);
        std::istringstream cmdstream(cmdstring, std::istringstream::in);
        std::string tmp;
        int maintime, byotime, byostones;
        
        cmdstream >> tmp >> maintime >> byotime >> byostones;
                
        // convert to centiseconds and set                
        game.set_timecontrol(maintime * 100, byotime * 100, byostones);
        
        gtp_printf(id, "");        
        return 1;
    } else if (!strncmp (command, "time_left", 9)) {
        std::string cmdstring(command);
        std::istringstream cmdstream(cmdstring, std::istringstream::in);
        std::string tmp, color;
        int time, stones;
        
        cmdstream >> tmp >> color >> time >> stones;
        
        int icolor;
        
        if (color == "w" || color == "white") {
            icolor = FastBoard::WHITE;
        } else if (color == "b" || color == "black") {
            icolor = FastBoard::BLACK;        
        } else {
            gtp_fail_printf(id, "Color in time adjust not understood.\n");
            return 1;
        }                                      
                                      
        game.adjust_time(icolor, time * 100, stones);
        
        gtp_printf(id, "");        
        return 1;
    } else if (!strcmp (command, "auto")) {    
        do {
            std::auto_ptr<UCTSearch> search(new UCTSearch(game));

            int move = search->think(game.get_to_move(), UCTSearch::PREFERPASS);
            game.play_move(move);  
            game.display_state();
                              
        } while (game.get_passes() < 2);

        return 1;
    } else if (!strcmp (command, "bench")) {
        Playout::do_playout_benchmark(game);
        return 1;
    } else if (!strcmp (command, "go")) {
        std::auto_ptr<UCTSearch> search(new UCTSearch(game));

        int move = search->think(game.get_to_move());
        game.play_move(move);                    

        game.move_to_text(move, vertex);            
        myprintf("%s\n", vertex);
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

