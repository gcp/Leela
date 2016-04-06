#include <vector>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <string>
#include <sstream>
#include <cmath>

#include "config.h"
#include "Utils.h"
#include "GameState.h"
#include "GTP.h"
#include "Playout.h"
#include "UCTSearch.h"
#include "SGFTree.h"
#include "AttribScores.h"
#include "Genetic.h"
#include "PNSearch.h"
#ifdef USE_NETS
#include "Network.h"
#endif

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
    "influence",
    "mc_score",
    "kgs-genmove_cleanup",
    "fixed_handicap",
    "place_free_handicap",
    "set_free_handicap",
    "loadsgf",    
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
    
    for (unsigned int i = 0; i < stringlist.size(); i++) {
        result += (i == 0 ? "" : "\n") + stringlist[i];
    }
    
    return result;                    
}

bool GTP::execute(GameState & game, std::string xinput) {            
    std::string input;
                    
    /* eat empty lines, simple preprocessing, lower case */
    for (unsigned int tmp = 0; tmp < xinput.size(); tmp++) {
        if (xinput[tmp] == 9) {
            input += " ";
        } else if ((xinput[tmp] > 0 && xinput[tmp] <= 9)
	        || (xinput[tmp] >= 11 && xinput[tmp] <= 31)
	        || xinput[tmp] == 127) {
	       continue;
        } else {        
            input += std::tolower(xinput[tmp]);
            //input += xinput[tmp];
        }            
        
        // eat multi whitespace
        if (input.size() > 1) {
            if (std::isspace(input[input.size() - 2]) &&
                std::isspace(input[input.size() - 1])) {
                input.resize(input.size() - 1);
            }
        }
    }    
    
    std::string command;    
    int id = -1;
    
    if (input == "") {
        return true;
    } else if (input == "exit") {
        exit(EXIT_SUCCESS);
        return true;
    } else if (input == "#") {
        return true;
    } else if (std::isdigit(input[0])) {        
        std::istringstream strm(input);
        char spacer;
        strm >> id;
        strm >> std::noskipws >> spacer;
        std::getline(strm, command);
    } else {
        command = input;
    }
    
    /* process commands */
    if (command == "protocol_version") {
        gtp_printf(id, "%d", GTP_VERSION);
        return true;
    } else if (command == "name") {
        gtp_printf(id, PROGRAM_NAME);
        return true;  
    } else if (command == "version") {
        gtp_printf(id, PROGRAM_VERSION);
        return true;
    } else if (command == "quit") {
        gtp_printf(id, "");
        exit(EXIT_SUCCESS);        
    } else if (command.find("known_command") == 0) {        
        std::istringstream cmdstream(command);
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
        return true;
    } else if (command.find("list_commands") == 0) {
        std::string outtmp(s_commands[0]);
        for (int i = 1; s_commands[i].size() > 0; i++) {            
            outtmp = outtmp + "\n" + s_commands[i];            
        }
        gtp_printf(id, outtmp.c_str());    
        return true;
    } else if (command.find("boardsize") == 0) {
        std::istringstream cmdstream(command);
        std::string stmp;
        int tmp;
        
        cmdstream >> stmp;  // eat boardsize
        cmdstream >> tmp;                
        
        if (!cmdstream.fail()) {        
            if (tmp < 2 || tmp > FastBoard::MAXBOARDSIZE) {
                gtp_fail_printf(id, "unacceptable size");
            } else {
                game.init_game(tmp);                
                gtp_printf(id, "");
            }
        } else {
            gtp_fail_printf(id, "syntax not understood");
        }
        
        return true;
    } else if (command.find("clear_board") == 0) {
        game.reset_game();
        gtp_printf(id, "");
        return true;
    } else if (command.find("komi") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp;
        float komi = 7.5f;
        
        cmdstream >> tmp;  // eat komi
        cmdstream >> komi;            
        
        if (!cmdstream.fail()) {
            game.set_komi(komi);
            gtp_printf(id, "");
        } else {
            gtp_fail_printf(id, "syntax not understood");
        }
        
        return true;
    } else if (command.find("play") == 0) {
        if (command.find("pass") != std::string::npos
            || command.find("resign") != std::string::npos) { 
            game.play_pass();
            gtp_printf(id, "");
        } else {
            std::istringstream cmdstream(command);
            std::string tmp;            
            std::string color, vertex;
            
            cmdstream >> tmp;   //eat play
            cmdstream >> color;
            cmdstream >> vertex;
            
            if (!cmdstream.fail()) {    
                if (!game.play_textmove(color, vertex)) {
                    gtp_fail_printf(id, "illegal move");
                } else {
                    gtp_printf(id, "");
                }
            } else {
                gtp_fail_printf(id, "syntax not understood");
            }
        } 
        return true;
    } else if (command.find("genmove") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp;
        
        cmdstream >> tmp;  // eat genmove
        cmdstream >> tmp;
        
        if (!cmdstream.fail()) {
            int who;
            if (tmp == "w" || tmp == "white") {
                who = FastBoard::WHITE;            
            } else if (tmp == "b" || tmp == "black") {
                who = FastBoard::BLACK;         
            } else {
                gtp_fail_printf(id, "syntax error");
                return 1;
            }   
            
            // start thinking
            {
                std::unique_ptr<UCTSearch> search(new UCTSearch(game));

                int move = search->think(who);
                game.play_move(who, move);                    

                std::string vertex = game.move_to_text(move);            
                gtp_printf(id, "%s", vertex.c_str());
            }
#ifdef USE_PONDER
            // now start pondering
            if (game.get_last_move() != FastBoard::RESIGN) {
                std::unique_ptr<UCTSearch> search(new UCTSearch(game));
                search->ponder();
            }                
#endif            
        } else {
            gtp_fail_printf(id, "syntax not understood");
        }
                
        return true;
    } else if (command.find("kgs-genmove_cleanup") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp;
        
        cmdstream >> tmp;  // eat kgs-genmove
        cmdstream >> tmp;
        
        if (!cmdstream.fail()) {
            int who;
            if (tmp == "w" || tmp == "white") {
                who = FastBoard::WHITE;            
            } else if (tmp == "b" || tmp == "black") {
                who = FastBoard::BLACK;         
            } else {
                gtp_fail_printf(id, "syntax error");
                return 1;
            } 
            
            game.set_passes(0);

            std::unique_ptr<UCTSearch> search(new UCTSearch(game));

            int move = search->think(who, UCTSearch::NOPASS);
            game.play_move(who, move);                    

            std::string vertex = game.move_to_text(move);             
            gtp_printf(id, "%s", vertex.c_str());
        } else {
            gtp_fail_printf(id, "syntax not understood");
        }
                
        return true;
    } else if (command.find("undo") == 0) {
        if (game.undo_move()) {
            gtp_printf(id, "");
        } else {
            gtp_fail_printf(id, "cannot undo");
        }            
        return true;
    } else if (command.find("showboard") == 0) {
        gtp_printf(id, "");
        game.display_state();
        return true;
    } else if (command.find("mc_score") == 0) {
        float ftmp = game.board.final_mc_score(game.get_komi());   
        /* white wins */        
        if (ftmp < -0.1) {
            gtp_printf(id, "W+%3.1f", (float)fabs(ftmp));
        } else if (ftmp > 0.1) {
            gtp_printf(id, "B+%3.1f", ftmp);
        } else {
            gtp_printf(id, "0");
        }                
        return true;
    } else if (command.find("final_score") == 0) {
        float ftmp = game.final_score();   
        /* white wins */        
        if (ftmp < -0.1) {
            gtp_printf(id, "W+%3.1f", (float)fabs(ftmp));
        } else if (ftmp > 0.1) {
            gtp_printf(id, "B+%3.1f", ftmp);
        } else {
            gtp_printf(id, "0");
        }                
        return true;
    } else if (command.find("final_status_list") == 0) {
        if (command.find("alive") != std::string::npos) {
            std::string livelist = get_life_list(game, true);
            gtp_printf(id, livelist.c_str());                        
        } else if (command.find("dead") != std::string::npos) {            
            std::string deadlist = get_life_list(game, false);
            gtp_printf(id, deadlist.c_str());                        
        } else {
            gtp_printf(id, "");
        }
        return true;
    } else if (command.find("time_settings") == 0) {        
        std::istringstream cmdstream(command);
        std::string tmp;
        int maintime, byotime, byostones;
        
        cmdstream >> tmp >> maintime >> byotime >> byostones;
                
        if (!cmdstream.fail()) {                
            // convert to centiseconds and set                
            game.set_timecontrol(maintime * 100, byotime * 100, byostones);
            
            gtp_printf(id, "");    
        } else {
            gtp_fail_printf(id, "syntax not understood");
        }    
        return true;
    } else if (command.find("time_left") == 0) {        
        std::istringstream cmdstream(command);
        std::string tmp, color;
        int time, stones;
        
        cmdstream >> tmp >> color >> time >> stones;
        
        if (!cmdstream.fail()) {
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
            
#ifdef USE_PONDER
            // KGS sends this after our move
            // now start pondering
            if (game.get_last_move() != FastBoard::RESIGN) {            
                std::unique_ptr<UCTSearch> search(new UCTSearch(game));
                search->ponder();
            }                
#endif            
        } else {
            gtp_fail_printf(id, "syntax not understood");
        }    
        return true;
    } else if (command.find("auto") == 0) {    
        do {
            std::unique_ptr<UCTSearch> search(new UCTSearch(game));

            int move = search->think(game.get_to_move(), UCTSearch::NORMAL);
            game.play_move(move);  
            game.display_state();
                              
        } while (game.get_passes() < 2);

        return true;
    } else if (command.find("bench") == 0) {
        Playout::do_playout_benchmark(game);
        return true;
    } else if (command.find("go") == 0) {
        std::unique_ptr<UCTSearch> search(new UCTSearch(game));

        int move = search->think(game.get_to_move());
        game.play_move(move);                    

        std::string vertex = game.move_to_text(move);        
        myprintf("%s\n", vertex.c_str());
        return true;
    } else if (command.find("influence") == 0) {
        gtp_printf(id, "");
        game.board.display_map(game.board.influence());        
        return true;
    } else if (command.find("fixed_handicap") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp;
        int stones;
        
        cmdstream >> tmp;   // eat fixed_handicap
        cmdstream >> stones;
        
        if (game.set_fixed_handicap(stones)) {
            std::string stonestring = game.board.get_stone_list();
            gtp_printf(id, "%s", stonestring.c_str());            
        } else {
            gtp_fail_printf(id, "Not a valid number of handicap stones");            
        }   
        return true;
    } else if (command.find("place_free_handicap") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp;
        int stones;
        
        cmdstream >> tmp;   // eat place_free_handicap
        cmdstream >> stones;
        
        game.place_free_handicap(stones);
        
        std::string stonestring = game.board.get_stone_list();
        gtp_printf(id, "%s", stonestring.c_str());
        
        return true;
    } else if (command.find("set_free_handicap") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp;        
        
        cmdstream >> tmp;   // eat set_free_handicap           
            
        do {    
            std::string vertex;
            
            cmdstream >> vertex;
            
            if (!cmdstream.fail()) {
                if (!game.play_textmove("black", vertex)) {
                    gtp_fail_printf(id, "illegal move");
                } else {
                    game.set_handicap(game.get_handicap() + 1);
                }                
            } 
        } while (!cmdstream.fail());                          
        
        std::string stonestring = game.board.get_stone_list();
        gtp_printf(id, "%s", stonestring.c_str());
        
        return true;
    } else if (command.find("loadsgf") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp, filename;
        int movenum;

        cmdstream >> tmp;   // eat loadsgf                
        
        cmdstream >> filename;        
        cmdstream >> movenum;
        
        if (cmdstream.fail()) {
            movenum = 999;
        }
        
        std::unique_ptr<SGFTree> sgftree(new SGFTree);
        
        sgftree->load_from_file(filename);                
        game = sgftree->get_mainline(movenum);
        
        gtp_printf(id, "");
        return true;
    } else if (command.find("tune") == 0) {
        std::istringstream cmdstream(command);
        std::string tmp, filename;

        cmdstream >> tmp;   // eat tune 

        cmdstream >> filename;        
        
        std::unique_ptr<AttribScores> scores(new AttribScores);
        
        scores->autotune_from_file(filename);
        
        gtp_printf(id, "");
        return true;
    } else if (command.find("genetune") == 0) {        
        std::unique_ptr<Genetic> genetic(new Genetic);
        
        genetic->genetic_tune();
        
        gtp_printf(id, "");
        return true;
    } else if (command.find("genesplit") == 0) {    
        std::istringstream cmdstream(command);
        std::string tmp, filename;

        cmdstream >> tmp;   // eat tune 
        cmdstream >> filename;          
        
        std::unique_ptr<Genetic> genetic(new Genetic);  
        genetic->genetic_split(filename);
        
        gtp_printf(id, "");
        return true;
    } else if (command.find("pn") == 0) {
        std::unique_ptr<PNSearch> pnsearch(new PNSearch(game));

        pnsearch->classify_groups();

        gtp_printf(id, "");
        return true;
    }  else if (command.find("nettune") == 0) {
#ifdef USE_NETS
        std::istringstream cmdstream(command);
        std::string tmp, filename;

        cmdstream >> tmp;   // eat nettune
        cmdstream >> filename;

        std::unique_ptr<Network> network(new Network);
        network->autotune_from_file(filename);
#endif
        gtp_printf(id, "");
        return true;
    } else if (command.find("predict") == 0) {
#ifdef USE_NETS
        auto vec = Network::get_Network()->get_scored_moves(&game);
#endif
        gtp_printf(id, "");
        return true;
    }

    gtp_fail_printf(id, "unknown command");
    return true;    
}
