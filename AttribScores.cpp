#include <cassert>
#include <list>
#include <set>
#include <fstream>
#include <boost/lexical_cast.hpp>

#include "config.h"

#include "Attributes.h"
#include "AttribScores.h"
#include "SGFTree.h"
#include "SGFParser.h"
#include "Utils.h"
#include "FastBoard.h"

using namespace Utils;

void AttribScores::gather_attributes(std::string filename, LearnVector & data) {
    int gametotal = SGFParser::count_games_in_file(filename);    
    myprintf("Total games in file: %d\n", gametotal);    

    int gamecount = 0;
    int allcount = 0;
    
    while (gamecount < gametotal) {        
        std::auto_ptr<SGFTree> sgftree(new SGFTree);        
        sgftree->load_from_file(filename, gamecount);                    
        
        int movecount = sgftree->count_mainline_moves();                
        
        SGFTree * treewalk = &(*sgftree); 
        int counter = 0;
        
        while (counter < movecount) {
            assert(treewalk != NULL);
            
            KoState * state = treewalk->get_state();
            int tomove = state->get_to_move();
            int move;
            
            if (treewalk->get_child(0) != NULL) {
                move = treewalk->get_child(0)->get_move(tomove);
                assert(move != 0);
            } else {
                break;
            }
            
            // sitting at a state, with the move actually played in move
            // gather feature sets of all moves
            std::vector<int> moves = state->generate_moves(tomove);            

            // make list of move - attributes pairs                       
            std::vector<int>::iterator it; 
            LrnPos position;                        
            bool moveseen = false;
            
            position.second.clear();
            position.second.reserve(moves.size() - 1);

            for(it = moves.begin(); it != moves.end(); ++it) {
                Attributes attributes;
                // gather attribute set of current move
                attributes.get_from_move(state, *it);
                
                if (*it == move) {
                    position.first = attributes;
                    moveseen = true;
                } else {                    
                    position.second.push_back(attributes);
                }                
            }                          
            
            if (moveseen) {
                allcount += position.second.size();
                data.push_back(position);         
            } else {
                myprintf("Mainline move not found: %d\n", move);
            }                
            
            counter++;
            treewalk = treewalk->get_child(0);            
        }    

        // Add 2 passes to game end 
        if (treewalk->get_state()->get_passes() == 0) {             
            KoState * state = treewalk->get_state();
            int tomove = state->get_to_move();
            std::vector<int> moves = state->generate_moves(tomove);            

            // make list of move - attributes pairs                       
            std::vector<int>::iterator it; 
            LrnPos position;                                    
                        
            position.second.reserve(moves.size() - 1);

            for(it = moves.begin(); it != moves.end(); ++it) {
                Attributes attributes;
                // gather attribute set of current move
                attributes.get_from_move(state, *it);
                
                if (*it == FastBoard::PASS) {
                    position.first = attributes;                    
                } else {                    
                    position.second.push_back(attributes);
                }                
            }                                      
            
            data.push_back(position);  
            
            state->play_move(FastBoard::PASS);
            moves = state->generate_moves(tomove);            

            position.second.clear();                        
            position.second.reserve(moves.size() - 1);

            for(it = moves.begin(); it != moves.end(); ++it) {
                Attributes attributes;
                // gather attribute set of current move
                attributes.get_from_move(state, *it);
                
                if (*it == FastBoard::PASS) {
                    position.first = attributes;                    
                } else {                    
                    position.second.push_back(attributes);                    
                }                
            }                                      

            allcount += position.second.size();
            
            data.push_back(position);  
        }
        
        gamecount++;                
        
        myprintf("Game %d, %3d moves, %d positions, %d allpos\n", 
                 gamecount, movecount, data.size(), allcount);                
    }
    
    myprintf("Gathering pass done.\n");
}

void AttribScores::autotune_from_file(std::string filename) {                    
    LearnVector data;

    gather_attributes(filename, data);
    
    // patterns worth learning
    std::set<int> goodpats;

    // start a new block for memory alloc reasons
    {
        // initialize the pattern list with a sparse map     
        std::map<int, int> patlist; 
        LearnVector::iterator it;

        for (it = data.begin(); it != data.end(); ++it) {
            int pat = it->first.get_pattern();
            
            patlist[pat]++;  

            /*AttrList::iterator ita;        

            for (ita = it->second.begin(); ita != it->second.end(); ++ita) {
                int pata = ita->get_pattern();

                patlist[pata]++;
            }*/
        }

        // reverse the map to a multimap
        std::multimap<int, int, std::greater<int> > revpatlist;
        std::map<int, int>::iterator itr;

        for (itr = patlist.begin(); itr != patlist.end(); ++itr) {
            int key = itr->first;
            int val = itr->second;

            revpatlist.insert(std::make_pair(val, key));
        }

        // print the multimap and make it a set of 
        // useful patterns
        std::multimap<int, int, std::greater<int> >::iterator itrr;        
        for (itrr = revpatlist.begin();itrr != revpatlist.end();++itrr) {
            int key = itrr->first;
            int val = itrr->second;
            
            if (key < 20) {
                break;
            }

            //myprintf("%7d %7d\n", key, val);             

            // add to good patterns list
            goodpats.insert(val);
        }
    }    

    myprintf("Good patterns: %d (reduced: %d)\n", goodpats.size(), goodpats.size()/16);

    // setup the weights
    m_weight.resize((1 << 16));
    fill(m_weight.begin(), m_weight.end(), 1.0f);    

    // now run the tuning:
    // for all convergence passes
    int pass = 0;
    while (1) {

        // for each parameter
        std::set<int>::iterator it;
        int pcount = 0;

        for (it = goodpats.begin(); it != goodpats.end(); ++it, ++pcount) {            
            int meidx = (*it);
            
            // prior
            int  wins = 1;            
            float sum = 2.0f / (1.0f + m_weight[meidx]);

            // gather all positions
            LearnVector::iterator posit;            
            for (posit = data.begin(); posit != data.end(); ++posit) {                
                
                float us = 1.0f;
                float them = us * m_weight[meidx];                
                
                AttrList::iterator ait;
                for (ait = posit->second.begin(); ait != posit->second.end(); ++ait) {
                    int pp = ait->get_pattern();
                   
                    if (goodpats.find(pp) != goodpats.end()) {
                        them += m_weight[pp];  
                    } else {
                        them += 1.0f;
                    }
                }

                sum += (us / them); 

                // only works for pattern right now
                if (posit->first.attribute_enabled(meidx)) {
                    wins++;                    
                }                
            }            
             // parameter modification
            float oldp = m_weight[*it];            
            float newp = (float)wins / (float)sum;
            myprintf("Parm %d, %d wins (out of %d), %f sum, %f -> %f\n", 
                      pcount, wins, data.size(), sum, oldp, newp); 
            m_weight[*it] = newp;
        }                    

        myprintf("Pass %d done\n", pass);
        
        std::ofstream fp_out;
        std::string fname = "param" + boost::lexical_cast<std::string>(pass) + ".txt";
        fp_out.open(fname.c_str());
        
        for (int i = 0; i < (1 << 16); i++) {
            fp_out << m_weight[i] << std::endl;
        }
        
        fp_out.close();
    }
}

// product of feature weights
float AttribScores::team_strength(Attributes & team) {
    int pattern = team.get_pattern();

    return m_weight[pattern];
}
