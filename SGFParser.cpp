#include <iostream>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <string>

#include "SGFParser.h"

// scan the file and extract the game with number index
std::string SGFParser::chop_from_file(std::string filename, int index) {
    std::ifstream ins(filename.c_str()); 
    std::string gamebuff;
    std::string result;
    
    if (ins.fail()) {
        throw new std::exception("Error opening file");
    }
    
    int count = 0;
    int nesting = 0;
    
    gamebuff.clear();

    while (!ins.eof() && count != index) {
        char c;
        ins >> c;
        
        gamebuff.push_back(c);
        
        if (c == '(') {
            if (nesting == 0) {
                // eat ; too
                ins >> c;  
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

std::string SGFParser::parse_property_name(std::istringstream & strm) {
    std::string result;
    
    while (!strm.eof()) {
        char c;
        strm >> c;
        
        if (!std::isupper(c)) {
            strm.unget();
            break;
        } else {
            result.push_back(c);
        }
    }
    
    return result;
}

std::string SGFParser::parse_property_value(std::istringstream & strm) {
    std::string result;
    
    while (!strm.eof()) {
        char c;
        strm >> c;

        if (!std::isspace(c)) {
            strm.unget();
            break;           
        }
    }
    
    char c;
    strm >> c;
    
    if (c != '[') {
        throw new std::exception("SGF Parse error: property starting [ not found");
    }
    
    while (!strm.eof()) {
        char c;
        strm >> c;
        
        if (c == ']') {
            break;
        } else if (c == '\\') {
            result.push_back(c);
            strm >> c;
        }        
        result.push_back(c);                
    }
    
    return result;
}

void SGFParser::parse(std::istringstream & strm, SGFTree * node) {    
    bool splitpoint = false;
    
    while (!strm.eof()) {
        char c;        
        strm >> c;
        
        if (strm.fail()) {
            return;
        }

        if (std::isspace(c)) {
            continue;
        }

        // parse a property
        if (std::isalpha(c) && std::isupper(c)) {
            strm.unget();
            
            std::string propname = parse_property_name(strm);            
            std::string propval  = parse_property_value(strm);            
            
            node->add_property(propname, propval);
                        
            continue;
        }
        
        if (c == '(') {
            // eat first ;
            char cc;
            strm >> cc;
            if (cc != ';') {
                strm.unget();
            }
            // start a variation here
            splitpoint = true;
            // new node
            std::auto_ptr<SGFTree> newnode(new SGFTree);
            SGFTree * newptr = node->add_child(*newnode);            
            parse(strm, newptr);
        } else if (c == ')') {
            // variation ends, go back
            // if the variation didn't start here, then
            // push the "variation ends" mark back
            // and try again one level up the tree
            if (!splitpoint) {
                strm.unget();
                return;
            } else {
                splitpoint = false;
                continue;
            }  
        } else if (c == ';') {
            // new node
            std::auto_ptr<SGFTree> newnode(new SGFTree);
            SGFTree * newptr = node->add_child(*newnode);            
            parse(strm, newptr);
        }
    }
}
