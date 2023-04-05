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
#include <boost/next_prior.hpp>

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
    size_t gametotal = games.size();
    size_t gamecount = 0;

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

        while (counter < movecount && counter < 40) {
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

    for(auto & it : hash_book) {
        if (it.second >= 200) {
            filtered_book.insert(it);
        }
    }

    hash_book.clear();
    myprintf("%d filtered positions\n", filtered_book.size());

    std::ofstream out("BookData.h");
    out << "static std::unordered_map<uint64, int> book_data{" << std::endl;
    for (auto it = filtered_book.cbegin(); it != filtered_book.cend(); ++it) {
        out << boost::format("{0x%08X, %d}") % it->first % it->second;
        if (boost::next(it) != filtered_book.cend()) {
            out << "," << std::endl;
        }
    }

    out << "};" << std::endl;

    out.close();
}

int Book::get_book_move(FastState & state) {
    auto moves = state.generate_moves(state.board.get_to_move());
    std::vector<std::pair<int, int>> scored_moves;
    std::vector<std::pair<int, int>> candidate_moves;

    int max_score = 0;
    for (auto & move : moves) {
        FastState currstate = state;

        if (move != FastBoard::PASS) {
            currstate.play_move(move);
            uint64 hash = currstate.board.get_canonical_hash();

            auto bid = book_data.find(hash);
            if (bid != book_data.end()) {
                max_score = std::max(max_score, bid->second);
                scored_moves.emplace_back(bid->second, move);
            }
        }
    }

    int cumul = 0;
    int threshold = max_score / 20;
    for (auto & candidate : scored_moves) {
        if (candidate.first > threshold) {
            cumul += candidate.first;
            candidate_moves.emplace_back(cumul, candidate.second);
        }
    }

#ifdef DUMP_BOOK
    std::sort(scored_moves.rbegin(), scored_moves.rend());
    for (auto & move : scored_moves) {
        if (move.first > threshold) {
            myprintf("%d %s\n",
                     move.first, state.move_to_text(move.second).c_str());
        }
    }
#endif

    int pick = Random::get_Rng()->randuint32(cumul);
    size_t candidate_count = candidate_moves.size();

    if (candidate_count > 0) {
        myprintf("%d book moves, %d total positions\n",
                 candidate_count, cumul);

        for (size_t i = 0; i < candidate_count; i++) {
            int point = candidate_moves[i].first;
            if (pick < point) {
                return candidate_moves[i].second;
            }
        }
    }

    return FastBoard::PASS;
}
