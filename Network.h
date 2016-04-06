#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <list>
#include <bitset>
#include <memory>
#include <caffe/caffe.hpp>

#include "tiny_cnn/tiny_cnn.h"
#include "FastState.h"

using namespace tiny_cnn;
using namespace tiny_cnn::activation;

// Move attributes
class Network {
public:
    typedef std::bitset<19*19> BoardPlane;
    typedef std::vector<BoardPlane> NNPlanes;
    typedef std::pair<int, NNPlanes> TrainPosition;
    typedef std::vector<TrainPosition> TrainVector;

    std::vector<std::pair<float, int>> get_scored_moves(FastState * state);
    void initialize();
    void autotune_from_file(std::string filename);
    static Network* get_Network(void);

private:
    typedef network<cross_entropy_multiclass, gradient_descent> NN;
    NN nn;
    std::unique_ptr<caffe::Net<float>> net;

    void gather_traindata(std::string filename, TrainVector& tv);
    void train_network(TrainVector& tv);
    static void gather_features(FastState * state, NNPlanes & planes, bool rotate = false);

    static Network* s_net;
};

#endif
