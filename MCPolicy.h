#ifndef MCPOLICY_H_INCLUDED
#define MCPOLICY_H_INCLUDED

#include "config.h"

#include <assert.h>
#include <array>
#include <vector>
#include <unordered_map>

class PolicyWeights {
public:
    static std::unordered_map<uint32_t, float> pattern_weights;
    static std::unordered_map<uint32_t, float> pattern_gradients;
    static std::array<float, 16> feature_weights;
    static std::array<float, 16> feature_gradients;

    static float get_pattern_weight(int pattern) {
        auto it = pattern_weights.find(pattern);

        if (it != pattern_weights.end()) {
            return it->second;
        } else {
            return 1.0f;
        }
    }

    static void set_pattern_weight(int pattern, float val) {
        auto it = pattern_weights.find(pattern);

        if (it != pattern_weights.end()) {
            it->second = val;
        } else {
            pattern_weights.insert(std::make_pair(pattern, val));
        }
    }

    static float get_pattern_gradient(int pattern) {
        auto it = pattern_gradients.find(pattern);

        if (it != pattern_gradients.end()) {
            return it->second;
        } else {
            return 0.0f;
        }
    }

    static void set_pattern_gradient(int pattern, float val) {
        auto it = pattern_gradients.find(pattern);

        if (it != pattern_gradients.end()) {
            it->second = val;
        } else {
            pattern_gradients.insert(std::make_pair(pattern, val));
        }
    }
};

// Scored features
constexpr int MWF_FLAG_PASS         = 1 <<  0;
constexpr int MWF_FLAG_NAKADE       = 1 <<  1;
constexpr int MWF_FLAG_PATTERN      = 1 <<  2;
constexpr int MWF_FLAG_SA           = 1 <<  3;
constexpr int MWF_FLAG_FORCING_SA   = 1 <<  4;
constexpr int MWF_FLAG_RANDOM       = 1 <<  5;
constexpr int MWF_FLAG_CRIT_MINE1   = 1 <<  6;
constexpr int MWF_FLAG_CRIT_MINE2   = 1 <<  7;
constexpr int MWF_FLAG_CRIT_HIS1    = 1 <<  8;
constexpr int MWF_FLAG_CRIT_HIS2    = 1 <<  9;
constexpr int MWF_FLAG_SAVING_1     = 1 << 10;
constexpr int MWF_FLAG_SAVING_2     = 1 << 11;
constexpr int MWF_FLAG_SAVING_3P    = 1 << 12;
constexpr int MWF_FLAG_CAPTURE_1    = 1 << 13;
constexpr int MWF_FLAG_CAPTURE_2    = 1 << 14;
constexpr int MWF_FLAG_CAPTURE_3P   = 1 << 15;
// Fake features
constexpr int MWF_FLAG_SAVING       = 1 << 29;
constexpr int MWF_FLAG_CAPTURE      = 1 << 30;

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
    bool has_bit(int bit) {
        assert(bit < 16);
        return m_flags & (1 << bit);
    }
    float get_score() {
        float result = 1.0f;
        for (int bit = 0; bit < 16; bit++) {
            if (has_bit(bit)) {
                result *= PolicyWeights::feature_weights[bit];
            }
        }
        result *= PolicyWeights::get_pattern_weight(m_pattern);
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
};

class MCPolicy {
public:
    static PolicyTrace policy_trace;

    static void add_to_trace(std::vector<MovewFeatures> & moves,
                             int chosen_idx) {
        policy_trace.trace.push_back(MoveDecision(moves, moves[chosen_idx]));
    }

    static void trace_process(int iterations, bool correct);
    static void adjust_weights();

    static void mse_from_file(std::string filename);
};

#endif