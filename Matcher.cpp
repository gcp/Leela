#include "config.h"

#include <limits>
#include <map>
// #define GEN_MPHF
#ifdef GEN_MPHF
#include <iostream>
#include <fstream>
#endif
#include "Matcher.h"
#include "FastBoard.h"
#include "Utils.h"
#include "MCPolicy.h"
#include "PatHash.h"

Matcher* Matcher::get_Matcher(void) {
    static Matcher s_matcher;
    return &s_matcher;
}

int Matcher::PatHashG(uint32 p) const {
    return (p ^= p >> 3) % G_SIZE;
}

int Matcher::PatHashV(uint32 d, uint32 p) const {
    p ^= Utils::rotl(p, 11) ^ ((p + d) >> 6);
    return p % V_SIZE;
}

int Matcher::PatIndex(uint32 pattern) const {
    auto g_v = G[PatHashG(pattern)];
    return PatHashV(g_v, pattern);
}

int Matcher::matches(int color, int pattern) const {
    auto idx = PatIndex(pattern);
    return m_patterns[color][idx];
}

// initialize matcher data
Matcher::Matcher() {
    constexpr int MAX_PAT_IDX = 1 << ((8 * 2) + (4 * 2));

    m_patterns[FastBoard::BLACK].resize(V_SIZE);
    m_patterns[FastBoard::WHITE].resize(V_SIZE);

    // Translates patterns to their index in the
    // valid patterns list.
    std::map<int, size_t> pattern_indexes;

    // Convert the map of valid patterns into a list
    // of indexes and weights of the exact minimal size
    for (auto & pat : PolicyWeights::pattern_map) {
        size_t pat_idx = pattern_indexes.size();
        // PolicyWeights::pattern_weights_sl[pat_idx] = pat.second;
        pattern_indexes.emplace(pat.first, pat_idx);
    }

    // minimal board we need is 3x3
    FastBoard board;
    board.reset_board(3);

#ifdef GEN_MPHF
    std::string filename = "patterns.txt";
    std::ofstream out(filename);
#endif

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

        int patindex = PatIndex(i);

        int reducpat1 = board.get_pattern3_augment_spec(startvtx, w, false);
        auto it1 = pattern_indexes.find(reducpat1);
        if (it1 != pattern_indexes.cend()) {
#ifdef GEN_MPHF
            out << i << std::endl;
#endif
            m_patterns[FastBoard::BLACK][patindex] = it1->second;
        }
        int reducpat2 = board.get_pattern3_augment_spec(startvtx, w, true);
        auto it2 = pattern_indexes.find(reducpat2);
        if (it2 != pattern_indexes.cend()) {
#ifdef GEN_MPHF
            out << i << std::endl;
#endif
            m_patterns[FastBoard::WHITE][patindex] = it2->second;
        }
    }
#ifdef GEN_MPHF
    out.close();
#endif
}
