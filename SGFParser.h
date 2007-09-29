#ifndef SGFPARSER_H_INCLUDED
#define SGFPARSER_H_INCLUDED

#include <string>

#include "SGFTree.h"

class SGFParser {
public:
    static std::string chop_from_file(std::string fname, int index);
    static void parse(std::istringstream & strm, SGFTree * node);
};


#endif