#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include "config.h"
#include <vector>
#include <string>
#include <list>
#include <bitset>

#include "tiny_cnn/tiny_cnn.h"

using namespace tiny_cnn;
using namespace tiny_cnn::activation;

// Move attributes
class Network {
public:
    void autotune_from_file(std::string filename);
private:
    typedef std::bitset<19*19> BoardPlane;
    typedef std::vector<BoardPlane> NNPlanes;
    typedef std::pair<int, NNPlanes> TrainPosition;
    typedef std::vector<TrainPosition> TrainVector;
    typedef network<cross_entropy_multiclass, gradient_descent> NN;
    NN nn;

    void gather_traindata(std::string filename, TrainVector& tv);
    void train_network(TrainVector& tv);
};

#endif
