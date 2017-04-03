#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <list>
#include <bitset>
#include <memory>
#include <array>

#ifdef USE_OPENCL
#include <atomic>
class UCTNode;
#endif
#ifdef USE_CAFFE
#include <caffe/caffe.hpp>
#endif

#include "FastState.h"

// Move attributes
class Network {
public:
    enum Ensemble {
        DIRECT, RANDOM_ROTATION, AVERAGE_ALL
    };
    using BoardPlane = std::bitset<19*19>;
    using NNPlanes = std::vector<BoardPlane>;
    using PredMoves = std::array<int, 3>;
    struct TrainPosition {
        NNPlanes planes;
        PredMoves moves;
        //float stm_score;
        //float stm_won;
        //float stm_score_tanh;
        //float stm_won_tanh;
    };
    using TrainVector = std::vector<TrainPosition>;
    using scored_node = std::pair<float, int>;
    using Netresult = std::vector<scored_node>;

    Netresult get_scored_moves(FastState * state,
                               Ensemble ensemble);
    float get_value(FastState *state,
                    Ensemble ensemble);
    static constexpr int CHANNELS_POLICY = 32;
    static constexpr int CHANNELS_VALUE = 24;
    static constexpr int MAX_CHANNELS = 192;
    static constexpr int MAX_VALUE_CHANNELS = 32;

#ifdef USE_OPENCL
    void async_scored_moves(std::atomic<int> * nodecount,
                            FastState * state, UCTNode * node,
                            Ensemble ensemble);
#endif
    void initialize();
    void benchmark(FastState * state);
    static void show_heatmap(FastState * state, Netresult & netres);
    void autotune_from_file(std::string filename);
    static Network* get_Network(void);
    std::string get_blas_backend();
    std::string get_opencl_backend();
    static int rotate_nn_idx(const int vertex, int symmetry);
    static int rev_rotate_nn_idx(const int vertex, int symmetry);

private:
#ifdef USE_CAFFE
    std::unique_ptr<caffe::Net<float>> net;
#endif

    Netresult get_scored_moves_internal(
      FastState * state, NNPlanes & planes, int rotation);
    float get_value_internal(
      FastState * state, NNPlanes & planes, int rotation);
    void gather_traindata(std::string filename, TrainVector& tv);
    void train_network(TrainVector& tv, size_t&, size_t&);
    static void gather_features_policy(FastState * state, NNPlanes & planes,
                                       BoardPlane** ladder = nullptr);
    static void gather_features_value(FastState * state, NNPlanes & planes,
                                      BoardPlane** ladder = nullptr);

    static Network* s_Net;
};

#endif
