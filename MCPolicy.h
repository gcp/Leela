#ifndef MCPOLICY_H_INCLUDED
#define MCPOLICY_H_INCLUDED

#include "config.h"

#include <assert.h>
#include <array>
#include <vector>
#include <unordered_map>

// Scored features
constexpr int NUM_PATTERNS = 8192;
constexpr int NUM_FEATURES = 23;
constexpr int MWF_FLAG_PASS         = 1 <<  0;
constexpr int MWF_FLAG_NAKADE       = 1 <<  1;
constexpr int MWF_FLAG_PATTERN      = 1 <<  2;
constexpr int MWF_FLAG_PATTERN_SA   = 1 <<  3;
constexpr int MWF_FLAG_SA           = 1 <<  4;
constexpr int MWF_FLAG_TOOBIG_SA    = 1 <<  5;
constexpr int MWF_FLAG_FORCE_SA     = 1 <<  6;
constexpr int MWF_FLAG_FORCEBIG_SA  = 1 <<  7;
constexpr int MWF_FLAG_RANDOM       = 1 <<  8;
constexpr int MWF_FLAG_CRIT_MINE1   = 1 <<  9;
constexpr int MWF_FLAG_CRIT_MINE2   = 1 << 10;
constexpr int MWF_FLAG_CRIT_MINE3   = 1 << 11;
constexpr int MWF_FLAG_CRIT_HIS1    = 1 << 12;
constexpr int MWF_FLAG_CRIT_HIS2    = 1 << 13;
constexpr int MWF_FLAG_CRIT_HIS3    = 1 << 14;
constexpr int MWF_FLAG_SAVING_SA    = 1 << 15;
constexpr int MWF_FLAG_SAVING_1     = 1 << 16;
constexpr int MWF_FLAG_SAVING_2     = 1 << 17;
constexpr int MWF_FLAG_SAVING_3P    = 1 << 18;
constexpr int MWF_FLAG_CAPTURE_1    = 1 << 19;
constexpr int MWF_FLAG_CAPTURE_2    = 1 << 20;
constexpr int MWF_FLAG_CAPTURE_3P   = 1 << 21;
constexpr int MWF_FLAG_SUICIDE      = 1 << 22;
// Fake features
constexpr int MWF_FLAG_SAVING       = 1 << 29;
constexpr int MWF_FLAG_CAPTURE      = 1 << 30;

class PolicyWeights {
public:
    static std::array<float, NUM_PATTERNS> pattern_gradients;
    static std::array<float, NUM_PATTERNS> pattern_weights;
    static std::array<float, NUM_FEATURES> feature_weights;
    static std::array<float, NUM_FEATURES> feature_gradients;
};

class MovewFeatures {
public:
    MovewFeatures(int vertex, int flags = 0, int target_size = 0)
        : m_vertex(vertex),
          m_flags(flags),
          m_target_size(target_size) {
    }
    int get_sq() {
        return m_vertex;
    }
    int get_flags() {
        return m_flags;
    }
    int get_target_size() {
        return m_target_size;
    }
    void add_flag(int flag) {
        m_flags |= flag;
    }
    void set_pattern(int pattern) {
        m_pattern = pattern;
    }
    int get_pattern() {
        return m_pattern;
    }
    bool has_bit(int bit) {
        assert(bit < NUM_FEATURES);
        return (m_flags & (1 << bit)) != 0;
    }
    bool is_pass() {
        return m_flags & MWF_FLAG_PASS;
    }
    float get_score() {
        float result = 1.0f;
        for (int bit = 0; bit < NUM_FEATURES; bit++) {
            if (has_bit(bit)) {
                result *= PolicyWeights::feature_weights[bit];
            }
        }
        if (!is_pass()) {
            result *= PolicyWeights::pattern_weights[m_pattern];
        }
        return result;
    }
private:
    // This is the actual move.
    int m_vertex;
    // Attributes/Features
    int m_flags;
    int m_target_size;
    int m_pattern;
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
        trace.push_back(MoveDecision(black_to_move, moves, moves[chosen_idx]));
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