#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <list>
#include <bitset>
#include <memory>
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
    typedef std::bitset<19*19> BoardPlane;
    typedef std::vector<BoardPlane> NNPlanes;
    typedef std::pair<int, NNPlanes> TrainPosition;
    typedef std::vector<TrainPosition> TrainVector;

    using scored_node = std::pair<float, int>;

    static const int CHANNELS = 22;
    static const int MAX_CHANNELS = 160;

    std::vector<scored_node> get_scored_moves(FastState * state);
#ifdef USE_OPENCL
    void async_scored_moves(boost::atomic<int> * nodecount,
                            FastState * state, UCTNode * node);
#endif
    void initialize();
    void benchmark(FastState * state);
    void show_heatmap(FastState * state, std::vector<scored_node>& moves);
    void autotune_from_file(std::string filename);
    static Network* get_Network(void);

private:
#ifdef USE_CAFFE
    std::unique_ptr<caffe::Net<float>> net;
#endif

    void gather_traindata(std::string filename, TrainVector& tv);
    void train_network(TrainVector& tv, size_t&, size_t&);
    static void gather_features(FastState * state, NNPlanes & planes);
    static int rotate_nn_idx(const int vertex, int symmetry);

    static Network* s_Net;
};

#endif
