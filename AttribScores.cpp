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

AttribScores* AttribScores::s_attribscores = 0;

AttribScores* AttribScores::get_attribscores(void) {
    if (s_attribscores == 0) {
        s_attribscores = new AttribScores;
        s_attribscores->load_from_file("param.dat");
    }
    
    return s_attribscores;
}

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
            position.second.reserve(moves.size());

            for(it = moves.begin(); it != moves.end(); ++it) {
                Attributes attributes;
                // gather attribute set of current move
                attributes.get_from_move(state, *it);
                
                position.second.push_back(attributes);
                
                if (*it == move) {
                    position.first = position.second.size() - 1;
                    moveseen = true;
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
                
                // gather attribute set of current move
                attributes.get_from_move(state, *it);

                position.second.push_back(attributes);                    

                if (*it == FastBoard::PASS) {
                    position.first = position.second.size() - 1;
                }                  
            }                                      
            
            data.push_back(position);  
            
            state->play_move(FastBoard::PASS);
            moves = state->generate_moves(tomove);            

            position.second.clear();                        
            position.second.reserve(moves.size());

            for(it = moves.begin(); it != moves.end(); ++it) {
                Attributes attributes;
                // gather attribute set of current move
                attributes.get_from_move(state, *it);
                
                position.second.push_back(attributes);                    
                
                if (*it == FastBoard::PASS) {
                    position.first = position.second.size() - 1;
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
    std::vector<int> goodpats;
        
    // start a new block for memory alloc reasons
    {
        // initialize the pattern list with a sparse map     
        std::map<int, int> patlist; 
        LearnVector::iterator it;

        for (it = data.begin(); it != data.end(); ++it) {            
            AttrList::iterator ita;        

            for (ita = it->second.begin(); ita != it->second.end(); ++ita) {
                int pata = ita->get_pattern();

                patlist[pata]++;
            }
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

            if (key < 1000) {
                break;
            }

            //myprintf("%7d %7d\n", key, val);             

            // add to good patterns list
            goodpats.push_back(val);            
        }
    }    

    myprintf("Good patterns: %d (reduced: %d)\n", goodpats.size(), goodpats.size()/16);

    // setup the weights
    m_pweight.resize((1 << 26));
    fill(m_pweight.begin(), m_pweight.end(), 1.0f);  
    
    m_fweight.resize(32);
    fill(m_fweight.begin(), m_fweight.end(), 1.0f); 

    // now run the tuning:
    // for all convergence passes
    int pass = 0;
    while (1) {
        {
            // for each parameter
            std::vector<int>::iterator it;
            int pcount = 0;

            for (it = goodpats.begin(); it != goodpats.end(); ++it, ++pcount) {            
                int meidx = (*it);
                
                // prior
                int  wins = 1;            
                float sum = 2.0f / (1.0f + m_pweight[meidx]);

                // gather all positions/competitions
                LearnVector::iterator posit;            
                for (posit = data.begin(); posit != data.end(); ++posit) {                                    
                    // teammates strength
                    float us = 0.0f;                    
                    // all participants strength
                    float them = 0.0f;                                         

                    // for each participating team                                                            
                    for (int k = 0; k < posit->second.size(); ++k) {
                        // are we in it? if so update teammates
                        if (posit->second[k].get_pattern() == meidx) {
                            // teammates
                            float teams = team_strength(posit->second[k]);
                            // remove us
                            teams = teams / m_pweight[meidx];
                            // total teammates
                            us += teams;
                            
                            // did we win?
                            if (posit->first == k) {
                                wins++;
                            }
                        }
                        // total opposition
                        them += team_strength(posit->second[k]);                                                                        
                    }

                    sum += (us / them);                                  
                }            
                 // parameter modification
                float oldp = m_pweight[meidx];            
                float newp = (float)wins / (float)sum;
                myprintf("PParm %d, %5d wins (out of %d), %f prob, %f -> %f\n", 
                          pcount, wins, data.size(), sum/(float)data.size(), oldp, newp); 
                m_pweight[meidx] = newp;
            }
        } 
        {                                 
            for (int pcount = 0; pcount < m_fweight.size(); ++pcount) {                            
                // prior
                int  wins = 1;            
                float sum = 2.0f / (1.0f + m_fweight[pcount]);

                // gather all positions
                LearnVector::iterator posit;            
                for (posit = data.begin(); posit != data.end(); ++posit) {                
                    // teammates strength
                    float us = 0.0f;                    
                    // all participants strength
                    float them = 0.0f;                                         

                    // for each participating team                                                            
                    for (int k = 0; k < posit->second.size(); ++k) {
                        // are we in it? if so update teammates
                        if (posit->second[k].attribute_enabled(pcount)) {
                            // teammates
                            float teams = team_strength(posit->second[k]);
                            // remove us
                            teams = teams / m_fweight[pcount];
                            // total teammates
                            us += teams;

                            // did we win?
                            if (posit->first == k) {
                                wins++;
                            }
                        }
                        // total opposition
                        them += team_strength(posit->second[k]);                                                                        
                    }

                    sum += (us / them);                                      
                }            
                // parameter modification
                float oldp = m_fweight[pcount];            
                float newp = (float)wins / (float)sum;
                myprintf("FParm %d, %5d wins (out of %d), %f prob, %f -> %f\n", 
                    pcount, wins, data.size(), sum/(float)data.size(), oldp, newp); 
                m_fweight[pcount] = newp;
            }           
        }
        
        pass++;       

        myprintf("Pass %d done\n", pass);
        
        std::ofstream fp_out;
        std::string fname = "param" + boost::lexical_cast<std::string>(pass) + ".txt";
        fp_out.open(fname.c_str());
        
        for (int i = 0; i < m_fweight.size(); i++) {
            fp_out << m_fweight[i] << std::endl;
        }
        
        for (int i = 0; i < goodpats.size(); i++) {
            int idx = goodpats[i];
            
            fp_out << idx << " " << m_pweight[idx] << std::endl;
        }
        
        fp_out.close();
    }
}

// product of feature weights
float AttribScores::team_strength(Attributes & team) {
    int pattern = team.get_pattern();

    float rating = m_pweight[pattern];
    
    for (int i = 0; i < m_fweight.size(); i++) {
        if (team.attribute_enabled(i)) {
            rating *= m_fweight[i];
        }        
    }
    
    return rating;
}


void AttribScores::load_from_file(std::string filename) {
    try {
        std::ifstream inf;

        inf.open(filename.c_str(), std::ifstream::in);

        if (!inf.is_open()) {
            throw std::exception("Error opening file");
        }

        m_pweight.clear();

        while (!inf.eof()) {
            float wt;
            inf >> wt;
            m_pweight.push_back(wt);
        }

        myprintf("%d weights loaded\n", m_pweight.size());
    } catch(std::exception & e) {
        myprintf("Error loading weights\n");
        throw e;
    } 
}