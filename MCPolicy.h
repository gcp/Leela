#ifndef MCPOLICY_H_INCLUDED
#define MCPOLICY_H_INCLUDED

#include "config.h"

#include <assert.h>
#include <array>
#include <vector>
#include <limits>
#include <unordered_map>

// Scored features
constexpr int NUM_PATTERNS = 8192;
constexpr int NUM_FEATURES = 23;
constexpr int MWF_FLAG_PASS         =  0;
constexpr int MWF_FLAG_NAKADE       =  1;
constexpr int MWF_FLAG_PATTERN      =  2;
constexpr int MWF_FLAG_PATTERN_SA   =  3;
constexpr int MWF_FLAG_SA           =  4;
constexpr int MWF_FLAG_TOOBIG_SA    =  5;
constexpr int MWF_FLAG_FORCE_SA     =  6;
constexpr int MWF_FLAG_FORCEBIG_SA  =  7;
constexpr int MWF_FLAG_RANDOM       =  8;
constexpr int MWF_FLAG_CRIT_MINE1   =  9;
constexpr int MWF_FLAG_CRIT_MINE2   = 10;
constexpr int MWF_FLAG_CRIT_MINE3   = 11;
constexpr int MWF_FLAG_CRIT_HIS1    = 12;
constexpr int MWF_FLAG_CRIT_HIS2    = 13;
constexpr int MWF_FLAG_CRIT_HIS3    = 14;
constexpr int MWF_FLAG_SAVING_SA    = 15;
constexpr int MWF_FLAG_SAVING_1     = 16;
constexpr int MWF_FLAG_SAVING_2     = 17;
constexpr int MWF_FLAG_SAVING_3P    = 18;
constexpr int MWF_FLAG_CAPTURE_1    = 19;
constexpr int MWF_FLAG_CAPTURE_2    = 20;
constexpr int MWF_FLAG_CAPTURE_3P   = 21;
constexpr int MWF_FLAG_SUICIDE      = 22;
// Fake features
constexpr int MWF_FLAG_SAVING       = 29;
constexpr int MWF_FLAG_CAPTURE      = 30;

class PolicyWeights {
public:
    alignas(64) static std::array<float, NUM_PATTERNS> pattern_gradients;
    alignas(64) static std::array<float, NUM_PATTERNS> pattern_weights;
    alignas(64) static std::array<float, NUM_FEATURES> feature_weights;
    alignas(64) static std::array<float, NUM_FEATURES> feature_gradients;
};

class MovewFeatures {
public:
    explicit MovewFeatures(int vertex, int flag, int target_size = 0)
        : m_vertex(vertex),
          m_target_size(target_size) {
        m_score = 1.0f;
        m_flags = 0;
        add_flag(flag);
    }
    int get_sq() {
        return m_vertex;
    }
    int get_target_size() {
        return m_target_size;
    }
    void add_flag(int flag) {
        assert(flag < std::numeric_limits<decltype(flag)>::digits);
        m_flags |= 1 << flag;
        if (flag < NUM_FEATURES) {
            m_score *= PolicyWeights::feature_weights[flag];
        }
    }
    void set_pattern(int pattern) {
        assert(pattern > 0);
        m_pattern = pattern;
        m_score *= PolicyWeights::pattern_weights[m_pattern];
    }
    int get_pattern() {
        assert(m_pattern > 0);
        return m_pattern;
    }
    bool has_flag(int flag) {
        assert(flag < std::numeric_limits<decltype(flag)>::digits);
        return (m_flags & (1 << flag)) != 0;
    }
    bool is_pass() {
        return has_flag(MWF_FLAG_PASS);
    }
    float get_score() {
        return m_score;
    }
private:
    // This is the actual move.
    int m_vertex;
    // Attributes/Features
    int m_flags;
    int m_target_size;
    int m_pattern;
    // Score so far
    float m_score;
};

class MoveDecision {
public:
    MoveDecision(bool p_black_to_move,
                 std::vector<MovewFeatures> & p_candidates,
                 MovewFeatures & p_pick)
        : black_to_move(p_black_to_move),
          candidates(p_candidates),
          pick(p_pick) {};
    bool black_to_move;
    std::vector<MovewFeatures> candidates;
    MovewFeatures pick;
};

class PolicyTrace {
public:
    std::vector<MoveDecision> trace;

    void add_to_trace(bool black_to_move,
                      std::vector<MovewFeatures> & moves,
                      int chosen_idx) {
        trace.emplace_back(black_to_move, moves, moves[chosen_idx]);
    }

    void trace_process(const int iterations, const float baseline,
                       const bool blackwin);
};

class MCPolicy {
public:
    static void hash_test(void);
    static void adjust_weights(float black_eval, float black_winrate);
    static void mse_from_file(std::string filename);
};

#endif