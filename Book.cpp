#include "config.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>
#include <array>
#include <iostream>
#include <fstream>
#include <boost/utility.hpp>
#include <boost/format.hpp>

#include "Book.h"
#include "Utils.h"
#include "SGFParser.h"
#include "SGFTree.h"
#include "Random.h"
#include "BookData.h"

using namespace Utils;

// #define DUMP_BOOK

void Book::bookgen_from_file(std::string filename) {
    std::unordered_map<uint64, int> hash_book;
    std::vector<std::string> games = SGFParser::chop_all(filename);
    int gametotal = games.size();
    int gamecount = 0;

    myprintf("Total games in file: %d\n", gametotal);

    while (gamecount < gametotal) {
        std::unique_ptr<SGFTree> sgftree(new SGFTree);

        try {
            sgftree->load_from_string(games[gamecount]);
        } catch (...) {
        };

        int movecount = sgftree->count_mainline_moves();

        SGFTree * treewalk = &(*sgftree);
        int counter = 0;

        while (counter < movecount) {
            assert(treewalk != NULL);
            assert(treewalk->get_state() != NULL);
            if (treewalk->get_state()->board.get_boardsize() != 19)
                break;

            KoState * state = treewalk->get_state();
            int tomove = state->get_to_move();
            int move;

            if (treewalk->get_child(0) != NULL) {
                move = treewalk->get_child(0)->get_move(tomove);
                if (move == SGFTree::EOT) {
                    break;
                    }
            } else {
                break;
            }

            uint64 canon_hash = state->board.get_canonical_hash();

            auto it = hash_book.find(canon_hash);
            if (it == hash_book.end()) {
                hash_book.insert(std::make_pair(canon_hash, 1));
            } else {
                it->second++;
            }

            counter++;
            treewalk = treewalk->get_child(0);
        }

        gamecount++;
        if (gamecount % 100 == 0) {
            myprintf("Game %d, %d total positions\n", gamecount, hash_book.size());
        }
    }

    games.clear();
    myprintf("%d total positions\n", hash_book.size());

    // Filter book
    std::map<uint64, int> filtered_book;

    for(auto it = hash_book.begin(); it != hash_book.end(); ++it) {
        if (it->second >= 10) {
            filtered_book.insert(*it);
        }
    }

    hash_book.clear();
    myprintf("%d filtered positions\n", filtered_book.size());

    std::ofstream out("BookData.h");
    out << "static std::unordered_map<uint64, int> book_data = {" << std::endl;
    for (auto it = filtered_book.begin(); it != filtered_book.end(); ++it) {
        out << boost::format("{0x%08X, %d}") % it->first % it->second;
        if (boost::next(it) != filtered_book.end()) {
            out << "," << std::endl;
        }
    }

    out << "};" << std::endl;

    out.close();
}

int Book::get_book_move(FastState & state) {
    auto moves = state.generate_moves(state.board.get_to_move());
    std::vector<std::pair<int, int>> candidate_moves;
    std::vector<std::pair<int, int>> display_moves;

    int cumul = 0;

    for (auto mit = moves.begin(); mit != moves.end(); ++mit) {
        FastState currstate = state;

        if (*mit != FastBoard::PASS) {
            currstate.play_move(*mit);
            uint64 hash = currstate.board.get_canonical_hash();

            auto bid = book_data.find(hash);
            if (bid != book_data.end()) {
                cumul += bid->second;
                candidate_moves.push_back(std::make_pair(*mit, cumul));
#ifdef DUMP_BOOK
                display_moves.push_back(std::make_pair(bid->second, *mit));
#endif
            }
        }
    }

#ifdef DUMP_BOOK
    std::sort(display_moves.rbegin(), display_moves.rend());
    for (int i = 0; i < display_moves.size(); i++) {
        myprintf("%d %s\n",
                 display_moves[i].first,
                 state.move_to_text(display_moves[i].second).c_str());
    }
#endif

    int pick = Random::get_Rng()->randint32(cumul);
    size_t candidate_count = candidate_moves.size();

    if (candidate_count > 0) {
        myprintf("%d book moves, %d total positions\n",
                 candidate_count, cumul);
    }

    for (size_t i = 0; i < candidate_count; i++) {
        int point = candidate_moves[i].second;
        if (pick < point) {
            return candidate_moves[i].first;
        }
    }

    return FastBoard::PASS;
}
