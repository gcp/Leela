#include <cassert>
#include <list>

#include "config.h"

#include "Attributes.h"
#include "AttribScores.h"
#include "SGFTree.h"
#include "SGFParser.h"
#include "Utils.h"
#include "FastBoard.h"

using namespace Utils;

void AttribScores::autotune_from_file(std::string filename) {        
    int gametotal = SGFParser::count_games_in_file(filename);    
 
    myprintf("Total games in file: %d\n", gametotal);
    
    typedef std::pair<Attributes, AttrList> LrnPos;
    typedef std::list<LrnPos> LearnVector;
    
    LearnVector data;
    
    int gamecount = 0;
    
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
                data.push_back(position);         
            } else {
                myprintf("Mainline move not found: %d\n", move);
            }                
            
            counter++;
            treewalk = treewalk->get_child(0);            
        }    
        
        gamecount++;                
        
        myprintf("Game %d, %3d moves, %d positions\n", gamecount, movecount, data.size());                
    }
    
    myprintf("Pass done.\n");
}
