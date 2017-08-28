#include "config.h"

#include <limits>
#include <unordered_map>
#include "Matcher.h"
#include "FastBoard.h"
#include "Utils.h"
#include "MCPolicy.h"
#include "Patterns.h"
#include "PatternHash.h"

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
    m_patterns[FastBoard::BLACK].resize(V_SIZE);
    m_patterns[FastBoard::WHITE].resize(V_SIZE);

#ifdef DEBUG
    // Crash
    unsigned short fill = std::numeric_limits<decltype(fill)>::max();
#else
    // Don't crash
    unsigned short fill = 0;
#endif
    std::fill(begin(m_patterns[0]), end(m_patterns[0]), fill);
    std::fill(begin(m_patterns[1]), end(m_patterns[1]), fill);

    std::unordered_map<int, size_t> pattern_indexes;

    // Convert the list of valid patterns into a list
    // of indexes of the exact minimal size
    for (auto const& pat : PolicyWeights::live_patterns) {
        size_t pat_idx = pattern_indexes.size();
        pattern_indexes.emplace(pat, pat_idx);
    }

    // minimal board we need is 3x3
    FastBoard board;
    board.reset_board(3);

    // center square
    int startvtx = board.get_vertex(1, 1);

    for (auto i : Matcher::Patterns) {
        int w = i;
        // fill board
        for (int k = 7; k >= 0; k--) {
            board.set_square(startvtx + board.get_extra_dir(k),
                             static_cast<FastBoard::square_t>(w & 3));
            w = w >> 2;
        }

        int reducpat1 = board.get_pattern3_augment_spec(startvtx, w, false);
        int reducpat2 = board.get_pattern3_augment_spec(startvtx, w, true);

        int pathash = PatIndex(i);

        auto it1 = pattern_indexes.find(reducpat1);
        if (it1 != pattern_indexes.cend()) {
            m_patterns[FastBoard::BLACK][pathash] = it1->second;
        }

        auto it2 = pattern_indexes.find(reducpat2);
        if (it2 != pattern_indexes.cend()) {
            m_patterns[FastBoard::WHITE][pathash] = it2->second;
        }
    }
}
