#include "config.h"

#include <limits>
#include <map>

#include "Matcher.h"
#include "FastBoard.h"
#include "Utils.h"
#include "MCPolicy.h"

Matcher* Matcher::s_matcher = nullptr;

Matcher* Matcher::get_Matcher(void) {
    if (!s_matcher) {
        s_matcher = new Matcher;
    }

    return s_matcher;
}

void Matcher::set_Matcher(Matcher * m) {
    if (s_matcher) {
        delete s_matcher;
    }

    s_matcher = m;
}

// initialize matcher data
Matcher::Matcher() {
    constexpr int MAX_PAT_IDX = 1 << ((8 * 2) + (4 * 2));

    m_patterns[FastBoard::BLACK].resize(MAX_PAT_IDX);
    m_patterns[FastBoard::WHITE].resize(MAX_PAT_IDX);

    // Translates patterns to their index in the
    // valid patterns list.
    std::map<int, size_t> pattern_indexes;

    // Convert the map of valid patterns into a list
    // of indexes and weights of the exact minimal size/
    for (auto & pat : PolicyWeights::pattern_map) {
        size_t pat_idx = pattern_indexes.size();
        PolicyWeights::pattern_weights_sl[pat_idx] = pat.second;
        pattern_indexes.emplace(pat.first, pat_idx);
    }

    // minimal board we need is 3x3
    FastBoard board;
    board.reset_board(3);

    // center square
    int startvtx = board.get_vertex(1, 1);

    for (int i = 0; i < MAX_PAT_IDX; i++) {
        int w = i;
        // fill board
        for (int k = 7; k >= 0; k--) {
            board.set_square(startvtx + board.get_extra_dir(k),
                             static_cast<FastBoard::square_t>(w & 3));
            w = w >> 2;
        }

        int reducpat1 = board.get_pattern3_augment_spec(startvtx, w, false);
        auto it1 = pattern_indexes.find(reducpat1);
        if (it1 != pattern_indexes.cend()) {
            m_patterns[FastBoard::BLACK][i] = it1->second;
        } else {
            m_patterns[FastBoard::BLACK][i] =
                std::numeric_limits<unsigned short>::max();
        }
        int reducpat2 = board.get_pattern3_augment_spec(startvtx, w, true);
        auto it2 = pattern_indexes.find(reducpat2);
        if (it2 != pattern_indexes.cend()) {
            m_patterns[FastBoard::WHITE][i] = it2->second;
        } else {
            m_patterns[FastBoard::WHITE][i] =
                std::numeric_limits<unsigned short>::max();
        }
    }
}
