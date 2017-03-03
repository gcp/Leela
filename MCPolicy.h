#ifndef MCPOLICY_H_INCLUDED
#define MCPOLICY_H_INCLUDED

#include "config.h"

#include <assert.h>
#include <array>
#include <vector>
#include <unordered_map>

// Scored features
constexpr int NUM_FEATURES = 20;
constexpr int MWF_FLAG_PASS         = 1 <<  0;
constexpr int MWF_FLAG_NAKADE       = 1 <<  1;
constexpr int MWF_FLAG_PATTERN      = 1 <<  2;
constexpr int MWF_FLAG_SA           = 1 <<  3;
constexpr int MWF_FLAG_TOOBIG_SA    = 1 <<  4;
constexpr int MWF_FLAG_FORCE_SA     = 1 <<  5;
constexpr int MWF_FLAG_FORCEBIG_SA  = 1 <<  6;
constexpr int MWF_FLAG_RANDOM       = 1 <<  7;
constexpr int MWF_FLAG_CRIT_MINE1   = 1 <<  8;
constexpr int MWF_FLAG_CRIT_MINE2   = 1 <<  9;
constexpr int MWF_FLAG_CRIT_MINE3   = 1 << 10;
constexpr int MWF_FLAG_CRIT_HIS1    = 1 << 11;
constexpr int MWF_FLAG_CRIT_HIS2    = 1 << 12;
constexpr int MWF_FLAG_CRIT_HIS3    = 1 << 13;
constexpr int MWF_FLAG_SAVING_1     = 1 << 14;
constexpr int MWF_FLAG_SAVING_2     = 1 << 15;
constexpr int MWF_FLAG_SAVING_3P    = 1 << 16;
constexpr int MWF_FLAG_CAPTURE_1    = 1 << 17;
constexpr int MWF_FLAG_CAPTURE_2    = 1 << 18;
constexpr int MWF_FLAG_CAPTURE_3P   = 1 << 19;
// Fake features
constexpr int MWF_FLAG_SAVING       = 1 << 29;
constexpr int MWF_FLAG_CAPTURE      = 1 << 30;

class PolicyWeights {
public:
    static std::unordered_map<int, float> pattern_gradients;
    static std::unordered_map<int, float> pattern_weights;
    static std::array<float, NUM_FEATURES> feature_weights;
    static std::array<float, NUM_FEATURES> feature_gradients;

    /* gradients default to 0.0 so everything just works,
       but weights need a default of 1.0
    */
    static float get_pattern_weight(int pattern) {
        assert(pattern >= 0);
        auto it = pattern_weights.find(pattern);

        if (it != pattern_weights.end()) {
            return it->second;
        } else {
            return 1.0f;
        }
    }

    static void set_pattern_weight(int pattern, float val) {
        assert(pattern >= 0);
        auto it = pattern_weights.find(pattern);

        if (it != pattern_weights.end()) {
            it->second = val;
        } else {
            pattern_weights.insert(std::make_pair(pattern, val));
        }
    }
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
        return m_flags & (1 << bit);
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
            result *= PolicyWeights::get_pattern_weight(m_pattern);
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
    MoveDecision(std::vector<MovewFeatures> & p_candidates,
                 MovewFeatures & p_pick)
        : candidates(p_candidates),
          pick(p_pick) {};
    std::vector<MovewFeatures> candidates;
    MovewFeatures pick;
};

class PolicyTrace {
public:
    std::vector<MoveDecision> trace;

    void add_to_trace(std::vector<MovewFeatures> & moves,
        int chosen_idx) {
        trace.push_back(MoveDecision(moves, moves[chosen_idx]));
    }

    void trace_process(int iterations, bool correct);
};

class MCPolicy {
public:
    static void adjust_weights();
    static void mse_from_file(std::string filename);
};

#endif