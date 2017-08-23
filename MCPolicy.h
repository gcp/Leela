#ifndef MCPOLICY_H_INCLUDED
#define MCPOLICY_H_INCLUDED

#include "config.h"

#include <assert.h>
#include <array>
#include <vector>
#include <limits>
#include <map>

// Scored features
constexpr int NUM_PATTERNS = 23518;
constexpr int NUM_FEATURES = 22;
constexpr int MWF_FLAG_PASS         =  0;
constexpr int MWF_FLAG_NAKADE       =  1;
constexpr int MWF_FLAG_PATTERN      =  2;
constexpr int MWF_FLAG_PATTERN_SA   =  3;
constexpr int MWF_FLAG_SA           =  4;
constexpr int MWF_FLAG_TOOBIG_SA    =  5;
constexpr int MWF_FLAG_FORCE_SA     =  6;
constexpr int MWF_FLAG_FORCEBIG_SA  =  7;
constexpr int MWF_FLAG_RANDOM       =  8;
constexpr int MWF_FLAG_SAVING_SA    =  9;
constexpr int MWF_FLAG_SAVING_1     = 10;
constexpr int MWF_FLAG_SAVING_2     = 11;
constexpr int MWF_FLAG_SAVING_3P    = 12;
constexpr int MWF_FLAG_SAVING_2_LIB = 13;
constexpr int MWF_FLAG_SAVING_3_LIB = 14;
constexpr int MWF_FLAG_SAVING_KILL  = 15;
constexpr int MWF_FLAG_CAPTURE_1    = 16;
constexpr int MWF_FLAG_CAPTURE_2    = 17;
constexpr int MWF_FLAG_CAPTURE_3P   = 18;
constexpr int MWF_FLAG_SUICIDE      = 19;
constexpr int MWF_FLAG_SEMEAI_2     = 20;
constexpr int MWF_FLAG_SEMEAI_3     = 21;

class PolicyWeights {
public:
    static std::map<int, float> pattern_map;
    alignas(64) static std::array<float, NUM_PATTERNS> pattern_gradients;
    alignas(64) static std::array<float, NUM_FEATURES> feature_gradients;
    alignas(64) static std::array<float, NUM_PATTERNS> pattern_weights;
    alignas(64) static std::array<float, NUM_FEATURES> feature_weights;
    alignas(64) static const std::array<float, NUM_PATTERNS> pattern_weights_sl;
    alignas(64) static const std::array<float, NUM_FEATURES> feature_weights_sl;
};

class MovewFeatures {
public:
    struct CaptureTag{};
    struct SavingTag{};
    explicit MovewFeatures(int vertex, int flag)
        : m_vertex(vertex) {
        add_flag(flag);
    }
    explicit MovewFeatures(int vertex, CaptureTag, int size)
        : m_vertex(vertex) {
        int flag;
        switch (size) {
        case 1:
            flag = MWF_FLAG_CAPTURE_1;
            break;
        case 2:
            flag = MWF_FLAG_CAPTURE_2;
            break;
        default:
            assert(size > 0);
            flag = MWF_FLAG_CAPTURE_3P;
            break;
        }
        add_flag(flag);
    }
    explicit MovewFeatures(int vertex, SavingTag, int size, int libs)
        : m_vertex(vertex) {
        int flag;
        switch (size) {
        case 1:
            flag = MWF_FLAG_SAVING_1;
            break;
        case 2:
            flag = MWF_FLAG_SAVING_2;
            break;
        default:
            assert(size > 0);
            flag = MWF_FLAG_SAVING_3P;
            break;
        }
        add_flag(flag);
        switch (libs) {
        case 0:
            assert(false);
            break;
        case 2:
            add_flag(MWF_FLAG_SAVING_2_LIB);
            break;
        case 3:
            add_flag(MWF_FLAG_SAVING_3_LIB);
            break;
        default:
            break;
        }
    }
    int get_sq() const {
        return m_vertex;
    }
    void add_flag(int flag) {
        assert(flag < std::numeric_limits<decltype(flag)>::digits);
        assert(flag < NUM_FEATURES);
        m_flags |= 1 << flag;
        m_score *= PolicyWeights::feature_weights[flag];
        //m_score *= PolicyWeights::feature_weights_sl[flag];
    }
    void set_pattern(int pattern) {
        assert(pattern < NUM_PATTERNS);
        m_pattern = pattern;
        m_score *= PolicyWeights::pattern_weights[m_pattern];
        //m_score *= PolicyWeights::pattern_weights_sl[m_pattern];
    }
    int get_pattern() const {
        assert(m_pattern > 0);
        return m_pattern;
    }
    bool has_flag(int flag) const {
        assert(flag < std::numeric_limits<decltype(flag)>::digits);
        return (m_flags & (1 << flag)) != 0;
    }
    bool is_pass() const {
        return has_flag(MWF_FLAG_PASS);
    }
    float get_score() const {
        return m_score;
    }
private:
    // This is the actual move.
    int m_vertex;
    // Attributes/Features
    int m_flags{0};
    int m_pattern;
    // Score so far
    float m_score{1.0f};
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

    void accumulate_sl_gradient(int & correct, int & picks);
};

class MCPolicy {
public:
    static void hash_test(void);
    static void adjust_weights(float black_eval, float black_winrate);
    static void mse_from_file(std::string filename);
    static void mse_from_file2(std::string filename);
};

#endif
