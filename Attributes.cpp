#include "config.h"

#include "Attributes.h"
#include "SGFTree.h"
#include "Utils.h"

using namespace Utils;

void Attributes::autotune_from_file(std::string filename) {        
    std::auto_ptr<SGFTree> sgftree(new SGFTree);
    
    sgftree->load_from_file(filename);                
    KoState state = sgftree->get_mainline(0);

    int count = sgftree->count_mainline_moves();
    
    myprintf("%d\n", count);    
    
}