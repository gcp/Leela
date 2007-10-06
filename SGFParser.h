#ifndef SGFPARSER_H_INCLUDED
#define SGFPARSER_H_INCLUDED

#include <string>
#include <sstream>

#include "SGFTree.h"

class SGFParser {
private:
    static std::string parse_property_name(std::istringstream & strm);
    static bool parse_property_value(std::istringstream & strm, std::string & result);
public:
    static std::string chop_from_file(std::string fname, int index);
    static void parse(std::istringstream & strm, SGFTree * node);  
    static int count_games_in_file(std::string filename);  
};


#endif