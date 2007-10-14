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
#include "Playout.h"

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
                        
            position.second.reserve(moves.size());
            
            {
                for(it = moves.begin(); it != moves.end(); ++it) {
                    Attributes attributes;
                    // gather attribute set of current move
                    attributes.get_from_move(state, *it);                                

                    position.second.push_back(attributes);                    

                    if (*it == FastBoard::PASS) {
                        position.first = position.second.size() - 1;
                    }                  
                }
            }                                      
            
            data.push_back(position);  
            
            state->play_move(FastBoard::PASS);
            moves = state->generate_moves(tomove);            

            position.second.clear();                        
            position.second.reserve(moves.size());
            
            {
                for(it = moves.begin(); it != moves.end(); ++it) {
                    Attributes attributes;
                    // gather attribute set of current move
                    attributes.get_from_move(state, *it);
                    
                    position.second.push_back(attributes);                    
                    
                    if (*it == FastBoard::PASS) {
                        position.first = position.second.size() - 1;
                    }                
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

            if (key < 5000) {
                break;
            }

            //myprintf("%7d %7d\n", key, val);             

            // add to good patterns list
            goodpats.push_back(val);            
        }
        
        myprintf("Good patterns: %d (original: %d)\n", goodpats.size(), patlist.size());
    }        

    // setup the weights    
    m_fweight.resize(64);
    fill(m_fweight.begin(), m_fweight.end(), 1.0f); 

    m_pat.clear();

    // now run the tuning:
    // for all convergence passes
    int pass = 0;
    while (1) {
        // pattern learning
        {                  
            // get total team strengths first      
            std::vector<float> allteams;
            LearnVector::iterator posit; 

            myprintf("Team gathering...");
            for (posit = data.begin(); posit != data.end(); ++posit) {            
                for (int k = 0; k < posit->second.size(); ++k) {
                    float teams = team_strength(posit->second[k]);
                    allteams.push_back(teams);
                }
            }
            myprintf("%d done\n", allteams.size());
            
            // for each parameter
            std::vector<int>::iterator it;
            int pcount = 0;            

            for (it = goodpats.begin(); it != goodpats.end(); ++it, ++pcount) {            
                int meidx = (*it);
                
                // prior
                int  wins = 1;            
                float sum = 2.0f / (1.0f + get_patweight(meidx));
                // restart team index
                int tcount = 0;

                // gather all positions/competitions                           
                for (posit = data.begin(); posit != data.end(); ++posit) {                      
                    // teammates strength
                    float us = 0.0f;                    
                    // all participants strength
                    float them = 0.0f;                                         

                    // for each participating team                                                            
                    for (int k = 0; k < posit->second.size(); ++k) {
                        // index into global team thing                                                             
                        float teams = allteams[tcount];
                        tcount++;                     
                        // total opposition
                        them += teams;
                        // are we in it? if so update teammates
                        if (posit->second[k].get_pattern() == meidx) {
                            // teammates
                            float mates = teams;
                            // remove us
                            mates = mates / get_patweight(meidx);
                            // total teammates
                            us += mates;
                            
                            // did we win?
                            if (posit->first == k) {
                                wins++;
                            }
                        }                       
                    }

                    sum += (us / them);                                  
                }            
                 // parameter modification
                float oldp = get_patweight(meidx);            
                float newp = (float)wins / (float)sum;
                myprintf("PParm %d, %5d wins (out of %d), %f prob, %f -> %f\n", 
                          pcount, wins, data.size(), sum/(float)data.size(), oldp, newp); 
                set_patweight(meidx, newp);
            }
        } 
        // feature parameter learning
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
                        float teams = team_strength(posit->second[k]);
                        // total opposition
                        them += teams;
                        // are we in it? if so update teammates
                        if (posit->second[k].attribute_enabled(pcount)) {
                            // teammates
                            float mates = teams;
                            // remove us
                            mates = mates / m_fweight[pcount];
                            // total teammates
                            us += mates;

                            // did we win?
                            if (posit->first == k) {
                                wins++;
                            }
                        }                        
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
            
            fp_out << idx << " " << get_patweight(idx) << std::endl;
        }
        
        fp_out.close();
    }
}

// product of feature weights
float AttribScores::team_strength(Attributes & team) {
    int pattern = team.get_pattern();
    
    float rating = get_patweight(pattern);            
    
    int sz = m_fweight.size();
    for (int i = 0; i < sz; i++) {
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
        
        m_fweight.clear();
        m_pat.clear();

        m_fweight.reserve(64);
        for (int i = 0; i < 64; i++) {
            float wt;
            inf >> wt;
            m_fweight.push_back(wt);
        }

        while (!inf.eof()) {
            int pat;
            float wt;
            inf >> pat >> wt;
            m_pat.insert(std::make_pair(pat, wt));
        }

        myprintf("%d feature weights loaded, %d patterns\n", 
                 m_fweight.size(), m_pat.size());
    } catch(std::exception & e) {
        myprintf("Error loading weights\n");
        throw e;
    } 
}

float AttribScores::get_patweight(int idx) {    
    std::map<int, float>::const_iterator it;
    float rating;

    it = m_pat.find(idx);

    if (it != m_pat.end()) {
        rating = it->second;
    } else {
        rating = 1.0f;
    }    

    return rating;
}

void AttribScores::set_patweight(int idx, float val) {
    std::map<int, float>::iterator it;

    it = m_pat.find(idx);

    if (it != m_pat.end()) {
        it->second = val;
    } else {
        m_pat.insert(std::make_pair(idx, val));
    }            
}
