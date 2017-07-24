#include "config.h"

#include <memory>
#include <cmath>
#include <unordered_set>
#include <string>
#include <iostream>
#include <fstream>
#include <omp.h>
#include <string.h>
#include "GTP.h"
#include "MCPolicy.h"
#include "SGFParser.h"
#include "SGFTree.h"
#include "Utils.h"
#include "Random.h"
#include "Network.h"
#include "Playout.h"

using namespace Utils;

#include "PolicyWeights.h"
alignas(64) std::array<float, NUM_PATTERNS> PolicyWeights::pattern_gradients;
alignas(64) std::array<float, NUM_FEATURES> PolicyWeights::feature_gradients;

// Adam
alignas(64) std::array<std::pair<float, float>, NUM_PATTERNS> pattern_adam{};
alignas(64) std::array<std::pair<float, float>, NUM_FEATURES> feature_adam{};
int t{0};

#if 0
void MCPolicy::hash_test() {
    std::atomic<int> mincol{4096};

    // 97 collisions 2148 -> 8192
    uint32 c1 = 0xc54be620;
    uint32 c2 = 0x7766d421;
    uint32 s1 = 26;
    uint32 s2 = 20;

    #pragma omp parallel for
    for (size_t inf = 0; inf < SIZE_MAX; inf++) {

        c1  = (uint32)Random::get_Rng()->random();
        c2  = (uint32)Random::get_Rng()->random();
        s1  = (uint32)Random::get_Rng()->randint16(32);
        s2  = (uint32)Random::get_Rng()->randint16(32);

        int pat_hash[8192];
        memset(pat_hash, 0, sizeof(pat_hash));
        int coll = 0;

        for (auto & patw : PolicyWeights::pattern_weights) {
            int pat = patw.first;
            uint32 h = pat;

            h  = Utils::rotl32(h, 11) ^ ((h + c1) >> s1);
            h *= c2;
            h ^= h >> s2;

            int hash = h & (8192 - 1);
            if (pat_hash[hash]) {
                coll++;
            }
            pat_hash[hash]++;
        }

        if (coll < mincol) {
            myprintf("c: %x %x s: %d %d, collisions: %d\n", c1, c2, s1, s2, coll);
            mincol = coll;
        }
    }

    exit(EXIT_SUCCESS);
}
#endif

void MCPolicy::mse_from_file(std::string filename) {
    std::vector<std::string> games = SGFParser::chop_all(filename);
    size_t gametotal = games.size();
    myprintf("Total games in file: %d\n", gametotal);

    omp_set_num_threads(cfg_num_threads);

    double sum_sq_pp = 0.0;
    double sum_sq_nn = 0.0;
    int count = 0;

    // PolicyWeights::feature_weights.fill(1.0f);
    // PolicyWeights::pattern_weights.fill(1.0f);
    Time start;

    while (1) {
        int pick = Random::get_Rng()->randint32(gametotal);

        std::unique_ptr<SGFTree> sgftree(new SGFTree);
        try {
            sgftree->load_from_string(games[pick]);
        } catch (...) {
        };

        int who_won = sgftree->get_winner();
        int handicap = sgftree->get_state()->get_handicap();

        int movecount = sgftree->count_mainline_moves();
        int move_pick = Random::get_Rng()->randint32(movecount);
        // GameState state = sgftree->follow_mainline_state(move_pick);
        KoState * state = sgftree->get_state_from_mainline(move_pick);

        if (who_won != FastBoard::BLACK && who_won != FastBoard::WHITE) {
            continue;
        }
        bool blackwon = (who_won == FastBoard::BLACK);

        constexpr int iterations = 512;
        PolicyWeights::feature_gradients.fill(0.0f);
        PolicyWeights::pattern_gradients.fill(0.0f);
        float bwins = 0.0f;
        float nwscore;
        float black_score;

        #pragma omp parallel
        {
            #pragma omp single nowait
            {
                FastState workstate = *state;
                nwscore = Network::get_Network()->get_value(
                    &workstate, Network::Ensemble::AVERAGE_ALL);
                if (workstate.get_to_move() == FastBoard::WHITE) {
                    nwscore = 1.0f - nwscore;
                }
                black_score = ((blackwon ? 1.0f : 0.0f) + nwscore) / 2.0f;
            }

            // Get EV (V)
            #pragma omp for reduction(+:bwins) schedule(dynamic, 8)
            for (int i = 0; i < iterations; i++) {
                FastState tmp = *state;

                Playout p;
                p.run(tmp, false, true, nullptr);

                float score = p.get_score();
                if (score > 0.0f) {
                    bwins += 1.0f / (float)iterations;
                }
            }

            // Policy Trace per thread
            #pragma omp for schedule(dynamic, 4)
            for (int i = 0; i < iterations; i++) {
                FastState tmp = *state;

                PolicyTrace policy_trace;
                Playout p;
                p.run(tmp, false, true, &policy_trace);

                bool black_won = p.get_score() > 0.0f;
                policy_trace.trace_process(iterations, bwins, black_won);
            }
        }

        MCPolicy::adjust_weights(black_score, bwins);

       // myprintf("n=%d BW: %d Score: %1.4f NN: %1.4f ",
       //          count, blackwon, bwins, nwscore);

        sum_sq_pp += std::pow((blackwon ? 1.0f : 0.0f) - bwins,   2.0f);
        sum_sq_nn += std::pow((blackwon ? 1.0f : 0.0f) - nwscore, 2.0f);

        count++;

        if (count % 10000 == 0) {
            Time end;
            float timediff = Time::timediff(start, end) / 100.0f;
            float ips = 10000.0f / timediff;
            start = end;
            myprintf("n=%d MSE MC=%1.4f MSE NN=%1.4f ips=%f\n",
                count,
                sum_sq_pp/10000.0,
                sum_sq_nn/10000.0,
                ips);
            sum_sq_pp = 0.0;
            sum_sq_nn = 0.0;
        }

        if (count % 10000 == 0) {
            std::string filename = "rltune_" + std::to_string(count) + ".txt";
            std::ofstream out(filename);
            for (int w = 0; w < NUM_FEATURES; w++) {
                out << PolicyWeights::feature_weights[w]
                    << "f, /* = " << w << " */"
                    << std::endl;
            }
            out << std::endl;
            for (int w = 0; w < NUM_PATTERNS; w++) {
                out << PolicyWeights::pattern_weights[w] << "f, " << std::endl;
            }
            out.close();
        }
    }
}

void PolicyTrace::trace_process(const int iterations, const float baseline,
                                const bool blackwon) {
    float z = 1.0f;
    if (!blackwon) {
        z = 0.0f;
    }
    float sign = z - baseline;

    if (trace.empty()) return;

    alignas(64) std::array<float, NUM_FEATURES> policy_feature_gradient{};
    alignas(64) std::array<float, NUM_PATTERNS> policy_pattern_gradient{};
    std::vector<float> candidate_scores;
    std::unordered_set<int> patterns;

    for (auto & decision : trace) {
        candidate_scores.clear();
        candidate_scores.reserve(decision.candidates.size());
        // get real probabilities
        float sum_scores = 0.0f;
        for (auto & mwf : decision.candidates) {
            float score = mwf.get_score();
            sum_scores += score;
            assert(!std::isnan(score));
            candidate_scores.push_back(score);
        }
        assert(sum_scores > 0.0f);
        assert(!std::isnan(sum_scores));
        // transform to probabilities
        for (float & score : candidate_scores) {
            score /= sum_scores;
        }

        // loop over features, get prob of feature
        std::array<float, NUM_FEATURES> feature_probabilities;
        for (size_t i = 0; i < NUM_FEATURES; i++) {
            float weight_prob = 0.0f;
            for (size_t c = 0; c < candidate_scores.size(); c++) {
                if (decision.candidates[c].has_bit(i)) {
                    weight_prob += candidate_scores[c];
                }
            }
            assert(!std::isnan(weight_prob));
            feature_probabilities[i] = weight_prob;
        }

        // get policy gradient
        for (int i = 0; i < NUM_FEATURES; i++) {
            float observed = 0.0f;
            if (decision.pick.has_bit(i)) {
                observed = 1.0f;
            }
            policy_feature_gradient[i] += sign *
                (observed - feature_probabilities[i]);
            assert(!std::isnan(policy_feature_gradient[i]));
        }

        patterns.clear();
        // get pat probabilities
        std::array<float, NUM_PATTERNS> pattern_probabilities;
        for (size_t c = 0; c < candidate_scores.size(); c++) {
            if (!decision.candidates[c].is_pass()) {
                int pat = decision.candidates[c].get_pattern();
                if (patterns.count(pat)) {
                    pattern_probabilities[pat] += candidate_scores[c];
                } else {
                    patterns.insert(pat);
                    pattern_probabilities[pat]  = candidate_scores[c];
                }
            }
        }

        for (int pat : patterns) {
            float observed = 0.0f;
            if (!decision.pick.is_pass()) {
                if (decision.pick.get_pattern() == pat) {
                    observed = 1.0f;
                }
            }
            policy_pattern_gradient[pat] += sign *
                (observed - pattern_probabilities[pat]);
        }
    }

    float positions = trace.size();
    float iters = iterations;
    assert(positions > 0.0f && iters > 0.0f);
    // scale by N*T
    float factor = 1.0f / (positions * iters);

    for (int i = 0; i < NUM_FEATURES; i++) {
        float grad = policy_feature_gradient[i] * factor;
        // accumulate total
        #pragma omp atomic
        PolicyWeights::feature_gradients[i] += grad;
    }

    for (int i = 0; i < NUM_PATTERNS; i++) {
        float grad = policy_pattern_gradient[i] * factor;
        #pragma omp atomic
        PolicyWeights::pattern_gradients[i] += grad;
    }
}

void MCPolicy::adjust_weights(float black_eval, float black_winrate) {
    constexpr float alpha = 0.01f;
    constexpr float beta_1 = 0.9f;
    constexpr float beta_2 = 0.999f;
    constexpr float delta = 1e-8f;

    // Timestep for Adam (total updates)
    t++;

    float Vdelta = black_eval - black_winrate;

    for (int i = 0; i < NUM_FEATURES; i++) {
        float orig_weight = PolicyWeights::feature_weights[i];
        float gradient = PolicyWeights::feature_gradients[i];

        feature_adam[i].first  = beta_1 * feature_adam[i].first
                                 + (1.0f - beta_1) * gradient;
        feature_adam[i].second = beta_2 * feature_adam[i].second
                                 + (1.0f - beta_2) * gradient * gradient;
        float bc_m1 = feature_adam[i].first  / (1.0f - (float)std::pow(beta_1, (double)t));
        float bc_m2 = feature_adam[i].second / (1.0f - (float)std::pow(beta_2, (double)t));
        float bc_m1_nag = beta_1 * bc_m1
                          + gradient * (1.0f - beta_1) / (1.0f - (float)std::pow(beta_1, (double)t));
        float adam_grad = alpha * bc_m1_nag / (std::sqrt(bc_m2) + delta);

        // Convert to theta
        float theta = std::log(orig_weight);
        theta += adam_grad * Vdelta;
        float gamma = std::exp(theta);
        assert(!std::isnan(gamma));
        gamma = std::max(gamma, 1e-6f);
        gamma = std::min(gamma, 1e6f);
        PolicyWeights::feature_weights[i] = gamma;
    }

    // Give the vectorizer a chance
    alignas(64) std::array<float, NUM_PATTERNS> adam_grad;
    for (int i = 0; i < NUM_PATTERNS; i++) {
        float gradient = PolicyWeights::pattern_gradients[i];
        pattern_adam[i].first  = beta_1 * pattern_adam[i].first
                                 + (1.0f - beta_1) * gradient;
        pattern_adam[i].second = beta_2 * pattern_adam[i].second
                                 + (1.0f - beta_2) * gradient * gradient;
        float bc_m1 = pattern_adam[i].first  / (1.0f - (float)std::pow(beta_1, (double)t));
        float bc_m2 = pattern_adam[i].second / (1.0f - (float)std::pow(beta_2, (double)t));
        float bc_m1_nag = beta_1 * bc_m1
                          + gradient * (1.0f - beta_1) / (1.0f - (float)std::pow(beta_1, (double)t));
        adam_grad[i] = alpha * bc_m1_nag / (std::sqrt(bc_m2) + delta);
    }

    for (int i = 0; i < NUM_PATTERNS; i++) {
        float orig_weight = PolicyWeights::pattern_weights[i];
        // Convert to theta
        float theta = std::log(orig_weight);
        theta += adam_grad[i] * Vdelta;
        float gamma = std::exp(theta);
        assert(!std::isnan(gamma));
        gamma = std::max(gamma, 1e-6f);
        gamma = std::min(gamma, 1e6f);
        PolicyWeights::pattern_weights[i] = gamma;
    }
}
