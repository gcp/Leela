#include <cassert>
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
    
    int gamecount = 0;
    
    while (gamecount < gametotal) {        
        std::auto_ptr<SGFTree> sgftree(new SGFTree);        
        sgftree->load_from_file(filename, gamecount);                    
        
        int movecount = sgftree->count_mainline_moves();
        
        myprintf("Game %d, %d moves\n", gamecount, movecount);
        
        SGFTree * treewalk = &(*sgftree); 
        int counter = 0;
        
        while (counter < movecount) {
            assert(treewalk != NULL);
            
            KoState * state = treewalk->get_state();
            int tomove = state->get_to_move();
            int move = treewalk->get_move(tomove);
            
            // sitting at a state, with the move actually played in move
            // gather feature sets of all moves
            std::vector<int> moves = state->generate_moves(tomove);            

            // make list of move - attributes pairs
            MoveAttrList moveattr;                        
            std::vector<int>::iterator it; 
            
            moveattr.reserve(moves.size());

            for(it = moves.begin(); it != moves.end(); ++it) {
                Attributes attributes;
                // gather attribute set of current move
                attributes.get_from_move(state, *it);
                // add pair to list
                moveattr.push_back(std::make_pair(*it, attributes));
            }                       
            
            counter++;
            treewalk = treewalk->get_child(0);            
        }    
        
        gamecount++;                
    }
    
    myprintf("Pass done.\n");
}
