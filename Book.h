#ifndef BOOK_H_INCLUDED
#define BOOK_H_INCLUDED

#include "config.h"
#include "FastState.h"

class Book {
public:
    static void bookgen_from_file(std::string filename);
    static int get_book_move(FastState & state);
};

#endif
