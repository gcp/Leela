#include <iostream>
#include <fstream>
#include <cctype>
#include <string>

#include "SGFParser.h"

// scan the file and extract the game with number index
std::string SGFParser::chop_from_file(std::string filename, int index) {
    std::ifstream ins(filename.c_str()); 
    std::string gamebuff;
    std::string result;
    
    int count = 0;
    int nesting = 0;
    
    gamebuff.clear();

    while (!ins.eof() && count != index) {
        char c;
        ins >> c;
        
        gamebuff.push_back(c);
        
        if (c == '(') {
            if (nesting == 0) {
                gamebuff.clear();                
            }
        
            nesting++;
        } else if (c == ')') {
            nesting--;
                        
            if (nesting == 0) {
                // one game processed
                count++;
                
                if (count == index) {
                    result = gamebuff;
                    result.erase(result.size() - 1);
                }
            }
        }                
    }

    ins.close();
    
    return result;
}

void SGFParser::parse(std::istringstream & strm, SGFTree * node) {    
    while (!strm.eof()) {
        char c;        
        strm >> c;

        if (std::isspace(c)) {
            continue;
            }

        // parse a property
        if (std::isupper(c)) {
            strm.unget();
        }
    }
}
