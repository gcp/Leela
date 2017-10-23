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
#include "GameState.h"

// Move attributes
class Network {
public:
    enum Ensemble {
        DIRECT, RANDOM_ROTATION, AVERAGE_ALL
    };
    using BoardPlane = std::bitset<19*19>;
    using NNPlanes = std::vector<BoardPlane>;
    struct TrainPosition {
        NNPlanes planes;
        int move;
        float stm_won_tanh;
    };
    using TrainVector = std::vector<TrainPosition>;
    using scored_node = std::pair<float, int>;
    using Netresult = std::pair<std::vector<scored_node>, float>;

    static Netresult get_scored_moves(GameState * state,
                                      Ensemble ensemble,
                                      int rotation = 0);
    static constexpr int INPUT_CHANNELS = 18;
    static constexpr int MAX_CHANNELS = 256;

    void initialize();
    void benchmark(GameState * state);
    static void show_heatmap(FastState * state, Netresult & netres, bool topmoves);
    void autotune_from_file(std::string filename);
    static Network* get_Network(void);
    std::string get_backend();
    static int rotate_nn_idx(const int vertex, int symmetry);
    static int rev_rotate_nn_idx(const int vertex, int symmetry);
    static void softmax(const std::vector<float>& input,
                        std::vector<float>& output,
                        float temperature = 1.0f);

private:
#ifdef USE_CAFFE
    static std::unique_ptr<caffe::Net> s_net;
#endif

    static Netresult get_scored_moves_internal(
      GameState * state, NNPlanes & planes, int rotation);
    void gather_traindata(std::string filename, TrainVector& tv);
    void train_network(TrainVector& tv, size_t&, size_t&);
    static void gather_features(GameState * state, NNPlanes & planes);
    static Network* s_Net;
};

#endif
