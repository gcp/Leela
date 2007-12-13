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

using namespace Utils;

float run_simulations(FastState & state, float res) {            
    int wins = 0;
    int runs = 0;
    
    for (int i = 0; i < 250; i++) {    
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

float run_testsets() {
    static const std::string prefix[] = { "win\\", "draw\\", "loss\\" };
    static const float values[] = { 1.0f, 0.5f, 0.0f };

    float se = 0.0f;
    int positions = 0;
    
    for (int j = 0; j < 3; j++) {
        for (int i = 1; i <= 100; i++) {        
            std::string file = prefix[j];
            file += boost::lexical_cast<std::string>(i);
            file += std::string(".sgf");
            
            std::auto_ptr<SGFTree> sgftree(new SGFTree);
            try {            
                sgftree->load_from_file(file);                
                int moves = sgftree->count_mainline_moves();
                if (moves > 0) {
                    FastState state = (*sgftree->get_state_from_mainline());   
                    //myprintf("Running %s\t", file.c_str());
                    se += run_simulations(state, values[j]);
                    positions++;
                }
            } catch (...) {
            }            
        }
    }
    
    float mse = se/(float)positions;        
    
    return mse;
}

void genetic_tune() {
    float bestmse = 1.0f;    
    
    typedef std::bitset<1 << 16> matchset;    
    
    std::vector<matchset> pool(300);
    std::vector<float> poolmse;
    poolmse.resize(pool.size());
    
    myprintf("Filling pool of %d entries...", pool.size());
    
    for (int i = 0; i < pool.size(); i++) {
        for (int j = 0; j < pool[i].size(); j++) {
            int bit1 = Random::get_Rng()->randint(2);            
            pool[i][j] = (bool)bit1;            
        }                
    }    
    
    myprintf("done\n");
    
    myprintf("Getting pool errors...\n");

    for (int i = 0; i < pool.size(); i++) {
        Matcher * matcher = new Matcher(pool[i]);
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
            // pick 2 random ancestors with s = 2
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
                    int bit1 = Random::get_Rng()->randint(2);                
                    newrank[i] = bit1;                
                }            
            }       
            
            // evaluate child
            Matcher * matcher = new Matcher(newrank);
            Matcher::set_Matcher(matcher);                             
        
            float err = run_testsets();                                                                                       
            
            if (err < bestmse) {
                bestmse = err;
                
                std::ofstream fp_out;
                std::string fname = "matcher_" + boost::lexical_cast<std::string>(err) + "_.txt";
                fp_out.open(fname.c_str());
            
                for (int i = 0; i < pool[element].size(); i++) {
                    fp_out << pool[element][i] << std::endl;
                }                  
                    
                fp_out.close();
            }                
            
            if (/*poolmse[element] - 0.00001f > bestmse*/ err < poolmse[element]) {
                if (err < poolmse[element]) {
                    myprintf("E: %3d OMSE: %f NMSE: %f BMSE: %f father %3d mother %3d Good\n", 
                             element, poolmse[element], err, bestmse, father, mother);
                } else {
                    myprintf("E: %3d OMSE: %f NMSE: %f BMSE: %f father %3d mother %3d Bad\n", 
                             element, poolmse[element], err, bestmse, father, mother);
                }
                pool[element] = newrank;
                poolmse[element] = err;            
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
