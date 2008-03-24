#include "config.h"

#include <memory>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <fstream>

#include "Genetic.h"
#include "SGFTree.h"
#include "GameState.h"
#include "Utils.h"
#include "Playout.h"
#include "Random.h"
#include "Matcher.h"
#include "SGFParser.h"
#include "UCTSearch.h"
#include "MCOTable.h"

using namespace Utils;

std::vector<float> Genetic::g_par(20+11, 1.0f);

float Genetic::run_simulations(FastState & state, float res) {            
    int wins = 0;
    int runs = 0;
    
    MCOwnerTable::clear();  
    Playout::mc_owner(state, 32); 
    
    FastState tmp;  
    int start_to_move = state.get_to_move();  
    
    for (int i = 0; i < 250; i++) {    
        tmp = state;        
                
        Playout p;        
        p.run(tmp, false);       
        
        float score = p.get_score();
        if (start_to_move == FastBoard::WHITE) {
            score = -score;
        }        
        if (score > 0.0) {
            wins++;
        }         
        runs++;
    }
    
    float simulscore = (float)wins/(float)runs;
    
    //myprintf("Simul: %f  Reference: %f\n", simulscore, res);
    
    float sqerr = powf(2.0f * (simulscore - res), 2.0f);
    
    return sqerr;
}

void Genetic::load_testsets() {
    static const std::string prefix[] = { "win\\", "loss\\", "draw\\" };
    static const float values[] = { 1.0f, 0.0f, 0.5f };

    float se = 0.0f;
    int positions = 0;
    
    for (int j = 0; j < 3; j++) {
        for (int i = 1; i <= 7000; i++) {        
            std::string file = prefix[j];
            file += boost::lexical_cast<std::string>(i);
            file += std::string(".sgf");
            
            std::auto_ptr<SGFTree> sgftree(new SGFTree);
            try {            
                sgftree->load_from_file(file);                
                int moves = sgftree->count_mainline_moves();
                if (moves > 0) {
                    FastState state = (*sgftree->get_state_from_mainline());   
                    if (values[j] > 0.75f) {
                        m_winpool.push_back(state);
                    } else if (values[j] < 0.25f) {
                        m_losepool.push_back(state);
                    } else {
                        m_drawpool.push_back(state);
                    }
                }
            } catch (...) {
            }            
        }
    }
    
    std::vector<FastState>(m_winpool).swap(m_winpool);
    std::vector<FastState>(m_losepool).swap(m_losepool);    
    std::vector<FastState>(m_drawpool).swap(m_drawpool);
    
    myprintf("Loaded %d winning, %d losing positions, %d drawn\n", 
             m_winpool.size(), m_losepool.size(), m_drawpool.size());
    
    return;
}

float Genetic::run_testsets() {    
    float ftmp;
    int loop;    
    float len;
    float score;
    GameState game;
    
    game.init_game(9, 7.5f);
    
    const int boardsize = game.board.get_boardsize();
    const int resign = (boardsize * boardsize) / 3;
    const int playoutlen = (boardsize * boardsize) * 2;    
    
    len = 0.0;
    score = 0;    
    
    for (loop = 0; loop < 20000; loop++) {
        do {                                    
            int move = game.play_random_move();                                                                   
        } while (game.get_passes() < 2 
                 && game.get_movenum() < playoutlen
                 && abs(game.estimate_mc_score()) < resign); 
                
        len += game.get_movenum();
        ftmp = game.calculate_mc_score();   
        score += ftmp;                
                
        game.reset_game();
    }
    
    return len/20000.0f;
}

void Genetic::genetic_tune() {
    // load the sets
    //load_testsets();
    
    float err = run_testsets();     
    myprintf("Starting MSE: %f\n", err);            
                
//    return;                

    // run the optimization
    float bestmse = 1000.0f;      
        
    typedef std::vector<float> paramset;    
    
    std::vector<paramset> pool(500);
    std::vector<float> poolmse;
    poolmse.resize(pool.size());
    
    myprintf("Filling pool of %d entries...", pool.size());
    
    for (int i = 0; i < pool.size(); i++) {
        pool[i].resize(31);
        for (int j = 0; j < pool[i].size(); j++) {            
            pool[i][j] = powf(10.0f, (((float)Random::get_Rng()->randint(20000)) / 10000.0f) - 1.0f);
        }                
    } 
    
    myprintf("done\n");
    
    myprintf("Getting pool errors...\n");

    for (int i = 0; i < pool.size(); i++) {
        g_par = pool[i];   
        
        Matcher::set_Matcher(new Matcher);                            
        
        poolmse[i] = run_testsets();
        
        if (poolmse[i] < bestmse) {
            bestmse = poolmse[i];
        }
        myprintf("MSE: %f BMSE: %f Pool %d\n", poolmse[i], bestmse, i);
    }    
    
    int generations = 0;

    do {
        for (int element = 0; element < pool.size(); element++) {                               
            // pick 2 random ancestors with s = 4
            float bestfather = 1000.0f;
            float bestmother = 1000.0f;
            int father;
            int mother;            
            
            for (int i = 0; i < 4; i++) {
                int select = Random::get_Rng()->randint(pool.size());
                if (poolmse[select] < bestfather) {
                    bestfather = poolmse[select];
                    father = select;
                }    
            }
            
            for (int i = 0; i < 4; i++) {
                int select = Random::get_Rng()->randint(pool.size());
                if (poolmse[select] < bestmother) {
                    bestmother = poolmse[select];
                    mother = select;
                }    
            }                                        
            
            paramset newrank;
            newrank.resize(31);
            
            // crossover/mutate
            for (int i = 0; i < newrank.size(); i++) {            
                int mutate = Random::get_Rng()->randint(20);
                if (mutate != 0) {
                    int cross = Random::get_Rng()->randint(2);
                    if (cross == 0) {
                        newrank[i] = pool[father][i];                    
                    } else {
                        newrank[i] = pool[mother][i];                    
                    }        
                } else {                    
                    newrank[i] = powf(10.0f, (((float)Random::get_Rng()->randint(20000)) / 10000.0f) - 1.0f);
                }            
            }       
            
            // evaluate child
            g_par = newrank;
                        
            Matcher::set_Matcher(new Matcher);
            
            float err = run_testsets();                                                                                       
            
            if (err < bestmse) {            
                bestmse = err;
                
                pool[element] = newrank;
                poolmse[element] = err;            
                
                std::ofstream fp_out;
                std::string fname = "matcher_" + boost::lexical_cast<std::string>(err) + ".txt";
                fp_out.open(fname.c_str());
            
                for (int i = 0; i < pool[element].size(); i++) {
                    fp_out << pool[element][i] << std::endl;
                }                  
                    
                fp_out.close();
            }                
            
            if (poolmse[element] - 0.00001f > bestmse) {
                if (err < poolmse[element]) {
                    myprintf("E: %3d OMSE: %f NMSE: %f BMSE: %f father %3d mother %3d Good\n", 
                             element, poolmse[element], err, bestmse, father, mother);
                             
                    pool[element] = newrank;
                    poolmse[element] = err;                                         
                } else {
                    myprintf("E: %3d OMSE: %f NMSE: %f BMSE: %f father %3d mother %3d Bad\n", 
                             element, poolmse[element], err, bestmse, father, mother);
                }                
            } else {
                myprintf("E: %3d OMSE: %f NMSE: %f BMSE: %f father %3d mother %3d ELITE\n", 
                         element, poolmse[element], err, bestmse, father, mother);
            }  
        }
        generations++;
        myprintf("New generation : %d\n", generations);                                      
    } while (bestmse > 0.0f);
    
    return;
}

void Genetic::genetic_split(std::string filename) {
    int gametotal = SGFParser::count_games_in_file(filename);    
    myprintf("Total games in file: %d\n", gametotal);    
    int gamecount = 0;    
    
    while (gamecount < gametotal) {        
        std::auto_ptr<SGFTree> sgftree(new SGFTree);        
        sgftree->load_from_file(filename, gamecount);                    
        
        int movecount = sgftree->count_mainline_moves();                                
        int counter = movecount - 7 - 1;
        bool written = false;
        
        GameState game = sgftree->get_mainline();
        
        while (counter > 7 && !written) {                        
            for (int i = 0; i < 7; i++) {
                game.undo_move();
            }
            counter -= 7;  
            game.trim_game_history(counter);
            
            int tomove = game.get_to_move();            
            
            std::auto_ptr<UCTSearch> search(new UCTSearch(game));
            int move = search->think(tomove, UCTSearch::NOPASS);
            
            float score = search->get_score();    
            
            myprintf("Score: %f\n", score);                           
            
            if (score > 0.66f && score < 0.90f) {            
                std::string gamestr = SGFTree::state_to_string(&game, FastBoard::BLACK);
                
                std::ofstream fp_out;
                std::string fname = "win\\" + boost::lexical_cast<std::string>(gamecount+1) + ".sgf";
                fp_out.open(fname.c_str()); 
                fp_out << gamestr;               
                fp_out.close();     
                
                myprintf("Dumping WIN\n");
                
                written = true;           
            } else if (score < 0.34f && score > 0.10f) {
                std::string gamestr = SGFTree::state_to_string(&game, FastBoard::BLACK);
                
                std::ofstream fp_out;
                std::string fname = "loss\\" + boost::lexical_cast<std::string>(gamecount+1) + ".sgf";
                fp_out.open(fname.c_str()); 
                fp_out << gamestr;               
                fp_out.close();     
                
                myprintf("Dumping LOSS\n");
                
                written = true;
            } else if ((score > 0.5f && score < 0.66f)
                       || (score < 0.5f && score > 0.34f)) {
                       
                myprintf("Skipping\n");
                                       
                written = true;                       
            }                        
        }   
        
        myprintf("GENETIC moving to next game\n");
        
        gamecount++;
    }                                                      
}