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

using namespace Utils;

float Genetic::run_simulations(FastState & state, float res) {            
    int wins = 0;
    int runs = 0;
    
    for (int i = 0; i < 100; i++) {    
        FastState tmp = state;
        int start_to_move = tmp.get_to_move();
        
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
    
    float sqerr = powf((simulscore - res), 2);
    
    return sqerr;
}

void Genetic::load_testsets() {
    static const std::string prefix[] = { "win\\", "loss\\" };
    static const float values[] = { 1.0f, 0.0f };

    float se = 0.0f;
    int positions = 0;
    
    for (int j = 0; j < 2; j++) {
        for (int i = 1; i <= 15000; i++) {        
            std::string file = prefix[j];
            file += boost::lexical_cast<std::string>(i);
            file += std::string(".sgf");
            
            std::auto_ptr<SGFTree> sgftree(new SGFTree);
            try {            
                sgftree->load_from_file(file);                
                int moves = sgftree->count_mainline_moves();
                if (moves > 0) {
                    FastState state = (*sgftree->get_state_from_mainline());   
                    if (values[j] > 0.5f) {
                        m_winpool.push_back(state);
                    } else {
                        m_losepool.push_back(state);
                    }                                                                                
                }
            } catch (...) {
            }            
        }
    }
    
    std::vector<FastState>(m_winpool).swap(m_winpool);
    std::vector<FastState>(m_losepool).swap(m_losepool);    
    
    myprintf("Loaded %d winning, %d losing positions\n", m_winpool.size(), m_losepool.size());
    
    return;
}

float Genetic::run_testsets() {    
    float se = 0.0f;
    int positions = 0;
    
    for (int i = 0; i < m_winpool.size(); i++) {
        FastState state = m_winpool[i];   
        //myprintf("Running %s\t", file.c_str());
        se += run_simulations(state, 1.0f);
        positions++;
    }
    
    for (int i = 0; i < m_losepool.size(); i++) {
        FastState state = m_losepool[i];   
        //myprintf("Running %s\t", file.c_str());
        se += run_simulations(state, 0.0f);
        positions++;
    }
    
    float mse = se/(float)positions;    
    myprintf("MSE: %f\n", mse);    
    
    return mse;
}

void Genetic::genetic_tune() {
    // load the sets
    load_testsets();
    
    float err = run_testsets();     
                
    return;                

    // run the optimization
    float bestmse = 1.0f;      
        
    typedef std::tr1::array<unsigned char, 65536> matchset;    
    
    std::vector<matchset> pool(500);
    std::vector<float> poolmse;
    poolmse.resize(pool.size());
    
    myprintf("Filling pool of %d entries...", pool.size());
    
    for (int i = 0; i < pool.size(); i++) {
        for (int j = 0; j < pool[i].size(); j++) {            
            pool[i][j] = Random::get_Rng()->randint(2);         
        }                
    } 
    
    myprintf("done\n");
    
    myprintf("Getting pool errors...\n");

    for (int i = 0; i < pool.size(); i++) {
        Matcher * matcher = new Matcher;//(pool[i]);
        Matcher::set_Matcher(matcher);                                        
        
        poolmse[i] = run_testsets();
        
        if (poolmse[i] < bestmse) {
            bestmse = poolmse[i];
        }
        myprintf("MSE: %f BMSE: %f Pool %d\n", poolmse[i], bestmse, i);
    }    
    
    int generations = 0;
    int element = 0;

    do {
        for (element = 0; element < pool.size(); element++) {                               
            // pick 2 random ancestors with s = 4
            float bestfather = 1.0f;
            float bestmother = 1.0f;
            int father;
            int mother;            
            
            for (int i = 0; i < 2; i++) {
                int select = Random::get_Rng()->randint(pool.size());
                if (poolmse[select] < bestfather) {
                    bestfather = poolmse[select];
                    father = select;
                }    
            }
            
            for (int i = 0; i < 2; i++) {
                int select = Random::get_Rng()->randint(pool.size());
                if (poolmse[select] < bestmother) {
                    bestmother = poolmse[select];
                    mother = select;
                }    
            }                                        
            
            matchset newrank;
            
            // crossover/mutate
            for (int i = 0; i < newrank.size(); i++) {            
                int mutate = Random::get_Rng()->randint(20);
                if (mutate != 19) {
                    int cross = Random::get_Rng()->randint(2);
                    if (cross == 0) {
                        newrank[i] = pool[father][i];                    
                    } else {
                        newrank[i] = pool[mother][i];                    
                    }        
                } else {                    
                    newrank[i] = Random::get_Rng()->randint(2);
                }            
            }       
            
            // evaluate child
            Matcher * matcher = new Matcher;//(newrank);
            Matcher::set_Matcher(matcher);                             
        
            float err = run_testsets();                                                                                       
            
            if (err < bestmse) {            
                bestmse = err;
                
                pool[element] = newrank;
                poolmse[element] = err;            
                
                std::ofstream fp_out;
                std::string fname = "matcher_" + boost::lexical_cast<std::string>(err) + "_.txt";
                fp_out.open(fname.c_str());
            
                for (int i = 0; i < pool[element].size(); i++) {
                    fp_out << (int)pool[element][i] << std::endl;
                }                  
                    
                fp_out.close();
            }                
            
            if (/*poolmse[element] - 0.00001f > bestmse*/1) {
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