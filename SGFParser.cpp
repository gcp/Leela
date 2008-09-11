#include <cassert>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <string>

#include "SGFParser.h"

//XXX: The scanners don't handle escape sequences
//XXX: make a char reading routine for this

// scan the file and extract the game with number index
std::string SGFParser::chop_from_file(std::string filename, int index) {
    std::ifstream ins(filename.c_str(), std::ifstream::binary | std::ifstream::in); 
    std::string gamebuff;
    std::string result;
    
    if (ins.fail()) {
        throw new std::exception("Error opening file");
    }
    
    ins >> std::noskipws;
    
    int count = 0;
    int nesting = 0;
    
    gamebuff.clear();

    char c;
    while (ins >> c && count <= index) {                
        
        if (count == index) {
            gamebuff.push_back(c);
        }
        
        if (c == '(') {
            if (nesting == 0) {
                // eat ; too
                ins >> c;  
                if (count == index) {
                    gamebuff.clear();
                }
            }
        
            nesting++;
        } else if (c == ')') {
            nesting--;
                        
            if (nesting == 0) {                
                if (count == index) {
                    result = gamebuff;
                    result.erase(result.size() - 1);
                }
                // one game processed
                count++;
            }
        }                
    }

    ins.close();
    
    return result;
}

std::string SGFParser::parse_property_name(std::istringstream & strm) {
    std::string result;
    
    char c;
    while (strm >> c) {        
        if (!std::isupper(c)) {
            strm.unget();
            break;
        } else {
            result.push_back(c);
        }
    }
    
    return result;
}

bool SGFParser::parse_property_value(std::istringstream & strm,
                                            std::string & result) {        
    strm >> std::noskipws;
    
    char c;
    while (strm >> c) {
        if (!std::isspace(c)) {
            strm.unget();
            break;           
        }
    }
    
    strm >> c;
    
    if (c != '[') {
        strm.unget();        
        return false;
    }
    
    while (strm >> c) {                        
        if (c == ']') {            
            break;                        
        } else if (c == '\\') {            
            strm >> c;
        }        
        result.push_back(c);                
    }
    
    strm >> std::skipws;
    
    return true;
}

void SGFParser::parse(std::istringstream & strm, SGFTree * node) {    
    bool splitpoint = false;
    
    char c;
    while (strm >> c) {                                
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
            bool success;
            
            do {                
                std::string propval;
                success = parse_property_value(strm, propval);
                if (success) {                                
                    node->add_property(propname, propval);                
                }                    
            } while (success);           
                        
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
            node = newptr;
            continue;
            //parse(strm, newptr);
        }
    }
}

int SGFParser::count_games_in_file(std::string filename) {
    std::ifstream ins(filename.c_str(), std::ifstream::binary | std::ifstream::in);         

    if (ins.fail()) {
        throw new std::exception("Error opening file");
    }

    int count = 0;
    int nesting = 0;    

    char c;
    while (ins >> c) {                
        if (c > 127 || c < 0) {
            do {
                ins >> c;
            } while (c > 127 || c < 0);
            continue;
        }

        if (c == '(') {            
            nesting++;
        } else if (c == ')') {
            nesting--;

            assert(nesting >= 0);

            if (nesting == 0) {
                // one game processed
                count++;
            }
        }                
    }

    ins.close();

    return count;
}
