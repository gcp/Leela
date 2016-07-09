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
#include <boost/atomic.hpp>
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
    using TrainPosition = std::pair<PredMoves, NNPlanes>;
    using TrainVector = std::vector<TrainPosition>;

    using scored_node = std::pair<float, int>;

    std::vector<scored_node> get_scored_moves(FastState * state,
                                              Ensemble ensemble);
    static constexpr int CHANNELS = 22;
    static constexpr int MAX_CHANNELS = 160;

#ifdef USE_OPENCL
    void async_scored_moves(boost::atomic<int> * nodecount,
                            FastState * state, UCTNode * node,
                            Ensemble ensemble);
#endif
    void initialize();
    void benchmark(FastState * state);
    static void show_heatmap(FastState * state, std::vector<scored_node>& moves);
    void autotune_from_file(std::string filename);
    static Network* get_Network(void);
    std::string get_backend();
    static int rotate_nn_idx(const int vertex, int symmetry);
    static int rev_rotate_nn_idx(const int vertex, int symmetry);

private:
#ifdef USE_CAFFE
    std::unique_ptr<caffe::Net<float>> net;
#endif

    std::vector<scored_node> get_scored_moves_internal(
        FastState * state, NNPlanes & planes, int rotation);
    void gather_traindata(std::string filename, TrainVector& tv);
    void train_network(TrainVector& tv, size_t&, size_t&);
    static void gather_features(FastState * state, NNPlanes & planes);

    static Network* s_Net;
};

#endif
