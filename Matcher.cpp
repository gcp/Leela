#include "config.h"

#include <unordered_map>
#include <math.h>

#include "Matcher.h"
#include "FastBoard.h"
#include "Utils.h"
#include "MCPolicy.h"

Matcher* Matcher::s_matcher = nullptr;

Matcher* Matcher::get_Matcher(void) {
    if (s_matcher == 0) {
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
    constexpr int MAX_PAT_IDX = 1 << ((8 * 2) + 4);

    m_patterns[FastBoard::BLACK].resize(MAX_PAT_IDX);
    m_patterns[FastBoard::WHITE].resize(MAX_PAT_IDX);

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
        reducpat1 = Utils::pattern_hash(reducpat1);
        int reducpat2 = board.get_pattern3_augment_spec(startvtx, w, true);
        reducpat2 = Utils::pattern_hash(reducpat2);

        m_patterns[FastBoard::BLACK][i] = reducpat1;
        m_patterns[FastBoard::WHITE][i] = reducpat2;
    }
}

int Matcher::matches(int color, int pattern) {
    return m_patterns[color][pattern];
}
