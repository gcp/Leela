#include "config.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <set>
#include <iostream>
#include <fstream>
#include <memory>
#include <cmath>
#include <array>
#include <boost/utility.hpp>
#include <boost/tr1/array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/format.hpp>

#ifdef USE_CAFFE
#include <caffe/proto/caffe.pb.h>
#include <caffe/util/db.hpp>
#include <caffe/util/io.hpp>
#include <caffe/blob.hpp>

using namespace caffe;
#endif
#ifdef USE_BLAS
#ifdef __APPLE__
#include <Accelerate.h>
#else
#ifdef _WIN32
#include <cblas.h>
#else
#include <openblas/cblas.h>
#endif
#endif
#include "Im2Col.h"
#endif
#ifdef USE_OPENCL
#include "OpenCL.h"
#include "UCTNode.h"
#endif

#include "SGFTree.h"
#include "SGFParser.h"
#include "Utils.h"
#include "FastBoard.h"
#include "Random.h"
#include "Network.h"

using namespace Utils;

Network* Network::s_Net = nullptr;

extern std::tr1::array<float, 102400> conv1_w;
extern std::tr1::array<float, 128> conv1_b;
extern std::tr1::array<float, 128> bn1_w1;
extern std::tr1::array<float, 128> bn1_w2;
extern std::tr1::array<float, 1>  bn1_w3;
extern std::tr1::array<float, 147456> conv2_w;
extern std::tr1::array<float, 128> conv2_b;
extern std::tr1::array<float, 128> bn2_w1;
extern std::tr1::array<float, 128> bn2_w2;
extern std::tr1::array<float, 1>  bn2_w3;
extern std::tr1::array<float, 147456> conv3_w;
extern std::tr1::array<float, 128> conv3_b;
extern std::tr1::array<float, 128> bn3_w1;
extern std::tr1::array<float, 128> bn3_w2;
extern std::tr1::array<float, 1>  bn3_w3;
extern std::tr1::array<float, 147456> conv4_w;
extern std::tr1::array<float, 128> conv4_b;
extern std::tr1::array<float, 128> bn4_w1;
extern std::tr1::array<float, 128> bn4_w2;
extern std::tr1::array<float, 1>  bn4_w3;
extern std::tr1::array<float, 147456> conv5_w;
extern std::tr1::array<float, 128> conv5_b;
extern std::tr1::array<float, 128> bn5_w1;
extern std::tr1::array<float, 128> bn5_w2;
extern std::tr1::array<float, 1>  bn5_w3;
extern std::tr1::array<float, 147456> conv6_w;
extern std::tr1::array<float, 128> conv6_b;
extern std::tr1::array<float, 128> bn6_w1;
extern std::tr1::array<float, 128> bn6_w2;
extern std::tr1::array<float, 1>  bn6_w3;
extern std::tr1::array<float, 147456> conv7_w;
extern std::tr1::array<float, 128> conv7_b;
extern std::tr1::array<float, 128> bn7_w1;
extern std::tr1::array<float, 128> bn7_w2;
extern std::tr1::array<float, 1>  bn7_w3;
extern std::tr1::array<float, 147456> conv8_w;
extern std::tr1::array<float, 128> conv8_b;
extern std::tr1::array<float, 128> bn8_w1;
extern std::tr1::array<float, 128> bn8_w2;
extern std::tr1::array<float, 1>  bn8_w3;
extern std::tr1::array<float, 147456> conv9_w;
extern std::tr1::array<float, 128> conv9_b;
extern std::tr1::array<float, 128> bn9_w1;
extern std::tr1::array<float, 128> bn9_w2;
extern std::tr1::array<float, 1>  bn9_w3;
extern std::tr1::array<float, 4608> conv10_w;
extern std::tr1::array<float, 4> conv10_b;
extern std::tr1::array<float, 3> bn10_w1;
extern std::tr1::array<float, 3> bn10_w2;
extern std::tr1::array<float, 1> bn10_w3;
extern std::tr1::array<float, 277248> ip11_w;
extern std::tr1::array<float, 256> ip11_b;
extern std::tr1::array<float, 256> bn11_w1;
extern std::tr1::array<float, 256> bn11_w2;
extern std::tr1::array<float, 1> bn11_w3;
extern std::tr1::array<float, 256> ip12_w;
extern std::tr1::array<float, 1> ip12_b;

Network * Network::get_Network(void) {
    if (!s_Net) {
        s_Net = new Network();
        s_Net->initialize();
    }
    return s_Net;
}

void Network::benchmark(FastState * state) {
    static const int BENCH_AMOUNT = 1000;
    Time start;

    for (int loop = 0; loop < BENCH_AMOUNT; loop++) {
        auto vec = get_scored_moves(state, Ensemble::RANDOM_ROTATION);
    }

    Time end;

    myprintf("%d predictions in %5.2f seconds -> %d p/s\n",
             BENCH_AMOUNT,
             (float)Time::timediff(start,end)/100.0,
             (int)((float)BENCH_AMOUNT/((float)Time::timediff(start,end)/100.0)));
}

void Network::initialize(void) {
#ifdef USE_OPENCL
    std::cerr << "Initializing OpenCL" << std::endl;
    OpenCL * cl = OpenCL::get_OpenCL();
    std::cerr << "Transfering weights to GPU..." << std::flush;
    cl->push_convolve(5, conv1_w, conv1_b);
    cl->push_batchnorm(bn1_w1, bn1_w2, bn1_w3);
    cl->push_convolve(3, conv2_w, conv2_b);
    cl->push_batchnorm(bn2_w1, bn2_w2, bn2_w3);
    cl->push_convolve(3, conv3_w, conv3_b);
    cl->push_batchnorm(bn3_w1, bn3_w2, bn3_w3);
    cl->push_convolve(3, conv4_w, conv4_b);
    cl->push_batchnorm(bn4_w1, bn4_w2, bn4_w3);
    cl->push_convolve(3, conv5_w, conv5_b);
    cl->push_batchnorm(bn5_w1, bn5_w2, bn5_w3);
    cl->push_convolve(3, conv6_w, conv6_b);
    cl->push_batchnorm(bn6_w1, bn6_w2, bn6_w3);
    cl->push_convolve(3, conv7_w, conv7_b);
    cl->push_batchnorm(bn7_w1, bn7_w2, bn7_w3);
    cl->push_convolve(3, conv8_w, conv8_b);
    cl->push_batchnorm(bn8_w1, bn8_w2, bn8_w3);
    cl->push_convolve(3, conv9_w, conv9_b);
    cl->push_batchnorm(bn9_w1, bn9_w2, bn9_w3);
    cl->push_convolve(3, conv10_w, conv10_b);
    std::cerr << "done" << std::endl;
#endif
#ifdef USE_BLAS
#ifndef __APPLE__
    openblas_set_num_threads(1);
    std::cerr << "BLAS Core: " << openblas_get_corename() << std::endl;
#endif
#endif
#ifdef USE_CAFFE
    myprintf("Initializing DCNN...");
    Caffe::set_mode(Caffe::GPU);

    net.reset(new Net<float>("model_5084.txt", TEST));
    net->CopyTrainedLayersFrom("model_5084.caffemodel");

    myprintf("Inputs: %d Outputs: %d\n",
        net->num_inputs(), net->num_outputs());

    Blob<float>* input_layer = net->input_blobs()[0];
    int num_channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    myprintf("Input: channels=%d, width=%d, height=%d\n", num_channels, width, height);

    for (int i = 0; i < net->num_outputs(); i++) {
        Blob<float>* output_layer = net->output_blobs()[i];
        int num_out_channels = output_layer->channels();
        width = output_layer->width();
        height = output_layer->height();
        myprintf("Output: channels=%d, width=%d, height=%d\n", num_out_channels, width, height);
    }

//#define WRITE_WEIGHTS
#ifdef WRITE_WEIGHTS
    std::ofstream out("weights.txt");
#endif

    int total_weights = 0;
    auto & layers = net->layers();
    myprintf("%d layers:\n", layers.size());
    int layer_num = 1;
    for (auto it = layers.begin(); it != layers.end(); ++it, ++layer_num) {
        myprintf("layer %d (%s)", layer_num, (*it)->type());
        auto & blobs = (*it)->blobs();
        if (blobs.size() > 0) myprintf(" = ");
        for (auto pars = blobs.begin(); pars != blobs.end(); ++pars) {
            const Blob<float> & blob = *(*pars);
            total_weights += blob.count();
            myprintf("%s ", blob.shape_string().c_str());
            if (boost::next(pars) != blobs.end()) myprintf("+ ");

#ifdef WRITE_WEIGHTS
            out << "// " << blob.shape_string() << std::endl;
            out << "std::tr1::array<float, " << blob.count()
                << "> weights = {{" << std::endl;
            for (int idx = 0; idx < blob.count(); idx++) {
                out << blob.cpu_data()[idx];
                if (idx != blob.count() - 1) out << ", ";
                else out << " }};" << std::endl;
            }
            out << std::endl;
#endif
        }
        myprintf("\n");
    }
#ifdef WRITE_WEIGHTS
    out.close();
#endif
    myprintf("%d total DCNN weights\n", total_weights);
#endif
}

#ifdef USE_BLAS
template<unsigned int filter_size,
         unsigned int channels, unsigned int outputs,
         unsigned long W, unsigned long B>
void convolve(std::vector<float>& input,
              std::tr1::array<float, W>& weights,
              std::tr1::array<float, B>& biases,
              std::vector<float>& output) {
    // fixed for 19x19
    constexpr unsigned int width = 19;
    constexpr unsigned int height = 19;
    constexpr unsigned int spatial_out = width * height;

    constexpr unsigned int filter_len = filter_size * filter_size;
    constexpr unsigned int filter_dim = filter_len * channels;

    std::vector<float> col(filter_dim * width * height);
    im2col<channels, filter_size>(input, col);

    // Weight shape (output, input, filter_size, filter_size)
    // 96 22 5 5
    // outputs[96,19x19] = weights[96,22x9] x col[22x9,19x19]
    // C←αAB + βC
    // M Number of rows in matrices A and C.
    // N Number of columns in matrices B and C.
    // K Number of columns in matrix A; number of rows in matrix B.
    // lda The size of the first dimention of matrix A; if you are
    // passing a matrix A[m][n], the value should be m.
    //    cblas_sgemm(CblasRowMajor, TransA, TransB, M, N, K, alpha, A, lda, B,
    //                ldb, beta, C, N);

    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                // M        N            K
                outputs, spatial_out, filter_dim,
                1.0f, &weights[0], filter_dim,
                &col[0], spatial_out,
                0.0f, &output[0], spatial_out);

    auto lambda_ReLU = [](float val) { return (val > 0.0f) ? val : 0.0f; };

    for (unsigned int o = 0; o < outputs; o++) {
        if (outputs > 4 || o > 0) {
            for (unsigned int b = 0; b < spatial_out; b++) {
                output[(o * spatial_out) + b] =
                    lambda_ReLU(biases[o] + output[(o * spatial_out) + b]);
            }
        }
    }
}

template<unsigned int inputs,
         unsigned int outputs,
         unsigned long W, unsigned long B>
void innerproduct(std::vector<float>& input,
                  std::tr1::array<float, W>& weights,
                  std::tr1::array<float, B>& biases,
                  std::vector<float>& output) {
    assert(B == outputs);

    cblas_sgemv(CblasRowMajor, CblasNoTrans,
                // M     K
                outputs, inputs,
                1.0f, &weights[0], inputs,
                &input[0], 1,
                0.0f, &output[0], 1);

    auto lambda_ReLU = [](float val) { return (val > 0.0f) ? val : 0.0f; };

    for (unsigned int o = 0; o < outputs; o++) {
        float val = biases[o] + output[o];
        if (outputs > 1) {
            val = lambda_ReLU(val);
        }
        output[o] = val;
    }
}
#endif

template<unsigned int channels,
         unsigned int spatial_size>
void batchnorm(std::vector<float>& input,
               std::tr1::array<float, channels>& means,
               std::tr1::array<float, channels>& variances,
               std::tr1::array<float, 1> scale,
               std::vector<float>& output)
{
    constexpr float epsilon = 1e-5;

    for (unsigned int c = 0; c < channels; ++c) {
        float mean = means[c] / scale[0];
        float variance = variances[c] / scale[0];
        variance += epsilon;
        float scale_stddiv = 1.0f / std::sqrt(variance);

        float * out = &output[c * spatial_size];
        float const * in  = &input[c * spatial_size];
        for (unsigned int b = 0; b < spatial_size; b++) {
            out[b] = scale_stddiv * (in[b] - mean);
        }
    }
}

void softmax(std::vector<float>& input,
             std::vector<float>& output) {
    assert(&input != &output);

    float alpha = *std::max_element(input.begin(),
                                    input.begin() + output.size());

    std::vector<float> helper(output.size());
    for (size_t i = 0; i < output.size(); i++) {
        helper[i] = std::exp(input[i] - alpha);
    }

    for (size_t i = 0; i < output.size(); i++) {
        float numer = helper[i];
        float denom = 0.0f;
        for (size_t j = 0; j < output.size(); j++) {
            denom += helper[j];
        }
        output[i] = numer / denom;
    }
}

#ifdef USE_OPENCL
class CallbackData {
public:
    boost::atomic<int> * m_nodecount;
    FastState m_state;
    UCTNode * m_node;
    int m_rotation;
    boost::atomic<int> * m_thread_results_outstanding;
    std::vector<float> m_output_data;
    std::vector<float> m_input_data;
};

extern "C" void CL_CALLBACK forward_cb(cl_event event, cl_int status,
                                       void* data) {
    CallbackData * cb_data = static_cast<CallbackData*>(data);

    // Mark the kernels as available
    cb_data->m_thread_results_outstanding->fetch_sub(1, boost::memory_order_release);

    constexpr int width = 19;
    constexpr int height = 19;
    std::vector<float> softmax_data(width * height);
    softmax(cb_data->m_output_data, softmax_data);
    std::vector<float>& outputs = softmax_data;

    Netresult result;

    for (size_t idx = 0; idx < outputs.size(); idx++) {
        int rot_idx = Network::rev_rotate_nn_idx(idx, cb_data->m_rotation);
        float val = outputs[rot_idx];
        int x = idx % 19;
        int y = idx / 19;
        int vtx = cb_data->m_state.board.get_vertex(x, y);
        if (cb_data->m_state.board.get_square(vtx) == FastBoard::EMPTY) {
            result.movescores.push_back(std::make_pair(val, vtx));
        }
    }

    // Network::show_heatmap(&cb_data->m_state, result);

    cb_data->m_node->expansion_cb(cb_data->m_nodecount, cb_data->m_state,
                                  result, true);

    delete cb_data;

    // Reduce the count of things having pointers to UCTNodes
    // or UCTSearch. We cannot destroy the search till these
    // have finished.
    OpenCL::get_OpenCL()->callback_finished();
}

void Network::async_scored_moves(boost::atomic<int> * nodecount,
                                 FastState * state,
                                 UCTNode * node,
                                 Ensemble ensemble) {
    if (state->board.get_boardsize() != 19) {
        return;
    }

    assert(ensemble == DIRECT || ensemble == RANDOM_ROTATION);
    int rotation;
    if (ensemble == RANDOM_ROTATION) {
        rotation = Random::get_Rng()->randint(8);
    } else {
        assert(ensemble == DIRECT);
        rotation = 0;
    }

    CallbackData * cb_data = new CallbackData();

    NNPlanes planes;
    gather_features(state, planes);

    constexpr int width = 19;
    constexpr int height = 19;

    cb_data->m_nodecount = nodecount;
    cb_data->m_state = *state;
    cb_data->m_node = node;
    cb_data->m_input_data.resize(Network::MAX_CHANNELS * 19 * 19);
    cb_data->m_output_data.resize(Network::MAX_CHANNELS * 19 * 19);
    cb_data->m_thread_results_outstanding =
        OpenCL::get_OpenCL()->get_thread_results_outstanding();
    //assert(cb_data->m_thread_result_outstanding.load(boost::memory_order_acquire) == 0);
    cb_data->m_rotation = rotation;

    for (int c = 0; c < Network::CHANNELS; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                int vtx = rotate_nn_idx(h * 19 + w, rotation);
                cb_data->m_input_data[(c * height + h) * width + w] =
                    (float)planes[c][vtx];
            }
        }
    }

    void * data = static_cast<void*>(cb_data);

    OpenCL::get_OpenCL()->forward_async(cb_data->m_input_data,
                                        cb_data->m_output_data,
                                        forward_cb, data);
}
#endif

Network::Netresult Network::get_scored_moves(
    FastState * state, Ensemble ensemble) {
    Netresult result;
    if (state->board.get_boardsize() != 19) {
        return result;
    }

    NNPlanes planes;
    gather_features(state, planes);

    if (ensemble == DIRECT) {
        result = get_scored_moves_internal(state, planes, 0);
    } else if (ensemble == RANDOM_ROTATION) {
        int rotation = Random::get_Rng()->randint(8);
        result = get_scored_moves_internal(state, planes, rotation);
    } else if (ensemble == AVERAGE_ALL) {
        result = get_scored_moves_internal(state, planes, 0);
        for (int r = 1; r < 8; r++) {
            auto sum_res = get_scored_moves_internal(state, planes, r);
            for (size_t i = 0; i < sum_res.movescores.size(); i++) {
                assert(result.movescores[i].second == sum_res.movescores[i].second);
                result.movescores[i].first += sum_res.movescores[i].first;
            }
            result.eval += sum_res.eval;
        }
        std::for_each(result.movescores.begin(), result.movescores.end(),
                      [](scored_node & sn){ sn.first /= 8.0f; });
        result.eval /= 8.0f;
    }

    if (ensemble == AVERAGE_ALL || ensemble == DIRECT) {
        show_heatmap(state, result);
    }

    return result;
}

Network::Netresult Network::get_scored_moves_internal(
    FastState * state, NNPlanes & planes, int rotation) {
    Netresult result;
#ifdef USE_CAFFE
    Blob<float>* input_layer = net->input_blobs()[0];
    int channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    assert(channels == (int)planes.size());
    assert(width == state->board.get_boardsize());
    assert(height == state->board.get_boardsize());
    float* input_data = input_layer->mutable_cpu_data();
#else
    constexpr int channels = CHANNELS;
    constexpr int width = 19;
    constexpr int height = 19;
    constexpr int max_channels = MAX_CHANNELS;
    std::vector<float> input_data(max_channels * width * height);
    std::vector<float> output_data(max_channels * width * height);
    std::vector<float> winrate_data(3 * width * height);
    std::vector<float> softmax_data(width * height);
#endif
    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                int vtx = rotate_nn_idx(h * 19 + w, rotation);
                input_data[(c * height + h) * width + w] =
                    (float)planes[c][vtx];
            }
        }
    }
#if defined(USE_BLAS)
    convolve<5,  32, 128>(input_data, conv1_w, conv1_b, output_data);
    batchnorm<128, 361>(output_data, bn1_w1, bn1_w2, bn1_w3, input_data);
    convolve<3, 128, 128>(input_data, conv2_w, conv2_b, output_data);
    batchnorm<128, 361>(output_data, bn2_w1, bn2_w2, bn2_w3, input_data);
    convolve<3, 128, 128>(input_data, conv3_w, conv3_b, output_data);
    batchnorm<128, 361>(output_data, bn3_w1, bn3_w2, bn3_w3, input_data);
    convolve<3, 128, 128>(input_data, conv4_w, conv4_b, output_data);
    batchnorm<128, 361>(output_data, bn4_w1, bn4_w2, bn4_w3, input_data);
    convolve<3, 128, 128>(input_data, conv5_w, conv5_b, output_data);
    batchnorm<128, 361>(output_data, bn5_w1, bn5_w2, bn5_w3, input_data);
    convolve<3, 128, 128>(input_data, conv6_w, conv6_b, output_data);
    batchnorm<128, 361>(output_data, bn6_w1, bn6_w2, bn6_w3, input_data);
    convolve<3, 128, 128>(input_data, conv7_w, conv7_b, output_data);
    batchnorm<128, 361>(output_data, bn7_w1, bn7_w2, bn7_w3, input_data);
    convolve<3, 128, 128>(input_data, conv8_w, conv8_b, output_data);
    batchnorm<128, 361>(output_data, bn8_w1, bn8_w2, bn8_w3, input_data);
    convolve<3, 128, 128>(input_data, conv9_w, conv9_b, output_data);
    batchnorm<128, 361>(output_data, bn9_w1, bn9_w2, bn9_w3, input_data);
    convolve<3, 128,   4>(input_data, conv10_w, conv10_b, output_data);
    softmax(output_data, softmax_data);

    std::vector<float>& outputs = softmax_data;

    // Now get the score
    // Skip move output
    std::copy(output_data.cbegin() + (width * height),
              output_data.cbegin() + (4 * width * height),
              winrate_data.begin());
    // BN, ReLU was done for the part of the conv data we use
    batchnorm<3, 361>(winrate_data, bn10_w1, bn10_w2, bn10_w3, input_data);
    innerproduct<3 * 361, 256>(input_data, ip11_w, ip11_b, winrate_data);
    batchnorm<256, 1>(winrate_data, bn11_w1, bn11_w2, bn11_w3, input_data);
    innerproduct<256,       1>(input_data, ip12_w, ip12_b, winrate_data);
    // Sigmoid
    winrate_data[0] = 1.0f / (1.0f + exp(-winrate_data[0]));
    result.eval = winrate_data[0];
#endif
#ifdef USE_OPENCL
    OpenCL::get_OpenCL()->forward(input_data, output_data);
    softmax(output_data, softmax_data);

    std::vector<float>& outputs = softmax_data;
#endif
#ifdef USE_CAFFE
    net->Forward();
    Blob<float>* output_layer = net->output_blobs()[0];
    const float* begin = output_layer->cpu_data();
    const float* end = begin + output_layer->channels();
    auto outputs = std::vector<float>(begin, end);
    Blob<float>* score_layer = net->output_blobs()[1];
    float winrate = score_layer->cpu_data()[0];
    result.eval = winrate;
#endif
    for (size_t idx = 0; idx < outputs.size(); idx++) {
        int rot_idx = rev_rotate_nn_idx(idx, rotation);
        float val = outputs[rot_idx];
        int x = idx % 19;
        int y = idx / 19;
        int vtx = state->board.get_vertex(x, y);
        if (state->board.get_square(vtx) == FastBoard::EMPTY) {
            result.movescores.push_back(std::make_pair(val, vtx));
        }
    }

    return result;
}

void Network::show_heatmap(FastState * state, Netresult& result) {
    auto moves = result.movescores;
    std::vector<std::string> display_map;
    std::string line;

    for (unsigned int y = 0; y < 19; y++) {
        for (unsigned int x = 0; x < 19; x++) {
            int vtx = state->board.get_vertex(x, y);

            auto item = std::find_if(moves.cbegin(), moves.cend(),
                [&vtx](scored_node const & item) {
                return item.second == vtx;
            });

            float score = 0.0f;
            // Non-empty squares won't be scored
            if (item != moves.end()) {
                score = item->first;
                assert(vtx == item->second);
            }

            line += boost::str(boost::format("%3d ") % int(score * 1000));
            if (x == 18) {
                display_map.push_back(line);
                line.clear();
            }
        }
    }

    for (int i = display_map.size() - 1; i >= 0; --i) {
        std::cerr << display_map[i] << std::endl;
    }

    std::stable_sort(moves.rbegin(), moves.rend());

    float cum = 0.0f;
    size_t tried = 0;
    while (cum < 0.85f && tried < moves.size()) {
        if (moves[tried].first < 0.01f) break;
        std::cerr << boost::format("%1.3f (") % moves[tried].first
            << state->board.move_to_text(moves[tried].second)
            << ")" << std::endl;
        cum += moves[tried].first;
        tried++;
    }

    std::cerr << "Eval: " << boost::format("%5.4f") % result.eval
              << std::endl;
}

void Network::gather_features(FastState * state, NNPlanes & planes) {
    planes.resize(32);
    BoardPlane& empt_color    = planes[0];
    BoardPlane& move_color    = planes[1];
    BoardPlane& othr_color    = planes[2];
    BoardPlane& libs_1        = planes[3];
    BoardPlane& libs_2        = planes[4];
    BoardPlane& libs_3        = planes[5];
    BoardPlane& libs_4        = planes[6];
    BoardPlane& libs_5        = planes[7];
    BoardPlane& libs_6p       = planes[8];
    BoardPlane& after_1       = planes[9];
    BoardPlane& after_2       = planes[10];
    BoardPlane& after_3       = planes[11];
    BoardPlane& after_4p      = planes[12];
    BoardPlane& after_1_e     = planes[13];
    BoardPlane& after_2_e     = planes[14];
    BoardPlane& after_3_e     = planes[15];
    BoardPlane& after_4p_e    = planes[16];
    BoardPlane& ladderloss    = planes[17];
    BoardPlane& ladderwin     = planes[18];
    BoardPlane& komove        = planes[19];
    BoardPlane& movehist1     = planes[20];
    BoardPlane& movehist2     = planes[21];
    BoardPlane& black_to_move = planes[22];
    BoardPlane& komi          = planes[23];
    BoardPlane& handicap2     = planes[24];
    BoardPlane& handicap3     = planes[25];
    BoardPlane& handicap4     = planes[26];
    BoardPlane& handicap5     = planes[27];
    BoardPlane& handicap6     = planes[28];
    BoardPlane& handicap7     = planes[29];
    BoardPlane& handicap8     = planes[30];
    BoardPlane& handicap9p    = planes[31];

    int tomove = state->get_to_move();

    if (tomove == FastBoard::BLACK) {
        black_to_move.set();
    }

    if (std::fabs(state->get_komi()) > 0.75f) {
        komi.set();
    }

    int handicap = state->get_handicap();
    if (handicap >= 2) {
        handicap2.set();
        if (handicap >= 3) {
            handicap3.set();
            if (handicap >= 4) {
                handicap4.set();
                if (handicap >= 5) {
                    handicap5.set();
                    if (handicap >= 6) {
                        handicap6.set();
                        if (handicap >= 7) {
                            handicap7.set();
                            if (handicap >= 8) {
                                handicap8.set();
                                if (handicap >= 9) {
                                    handicap9p.set();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // collect white, black occupation planes
    for (int j = 0; j < 19; j++) {
        for(int i = 0; i < 19; i++) {
            int vtx = state->board.get_vertex(i, j);
            FastBoard::square_t color =
                state->board.get_square(vtx);
            int idx = j * 19 + i;
            if (color != FastBoard::EMPTY) {
                if (color == tomove) {
                    move_color[idx] = true;
                } else {
                    othr_color[idx] = true;
                }
                int rlibs = state->board.count_rliberties(vtx);
                if (rlibs == 1) {
                    libs_1[idx] = true;
                } else if (rlibs == 2) {
                    libs_2[idx] = true;
                } else if (rlibs == 3) {
                    libs_3[idx] = true;
                } else if (rlibs == 4) {
                    libs_4[idx] = true;
                } else if (rlibs == 5) {
                    libs_5[idx] = true;
                } else if (rlibs >= 6) {
                    libs_6p[idx] = true;
                }
            } else {
                empt_color[idx] = true;

                std::pair<int, int> p =
                    state->board.after_liberties(tomove, vtx);
                int al = p.first;
                int at = p.second;
                if (al == 1) {
                    after_1[idx] = true;
                } else if (al == 2) {
                    after_2[idx] = true;
                } else if (al == 3) {
                    after_3[idx] = true;
                } else if (al >= 4) {
                    after_4p[idx] = true;
                }
                if (at == 1) {
                    after_1_e[idx] = true;
                } else if (at == 2) {
                    after_2_e[idx] = true;
                } else if (at == 3) {
                    after_3_e[idx] = true;
                } else if (at >= 4) {
                    after_4p_e[idx] = true;
                }

                int ss = state->board.saving_size(tomove, vtx);
                if (ss > 0) {
                    int ae = state->board.count_pliberties(vtx);
                    if (ae == 2) {
                        if (state->board.check_losing_ladder(tomove, vtx)) {
                            //std::cerr << "losing ladder: "
                            //          << state->board.move_to_text(state->board.get_vertex(i, j))
                            //          << std::endl;
                            ladderloss[idx] = true;
                        }
                    }
                }
                bool wl = state->board.check_winning_ladder(tomove, vtx);
                if (wl) {
                    ladderwin[idx] = true;
                    //std::cerr << "winning ladder: "
                    //          << state->board.move_to_text(state->board.get_vertex(i, j))
                    //          << std::endl;

                }
            }
        }
    }

    if (state->get_last_move() > 0) {
        std::pair<int, int> lastmove = state->board.get_xy(state->get_last_move());
        int idx = lastmove.second * 19 + lastmove.first;
        movehist1[idx] = true;
        if (state->get_prevlast_move() > 0) {
            std::pair<int, int> prevlast = state->board.get_xy(state->get_prevlast_move());
            int idxp = prevlast.second * 19 + prevlast.first;
            movehist2[idxp] = true;
        }
    }

    if (state->get_komove() > 0) {
        std::pair<int, int> kosq = state->board.get_xy(state->get_komove());
        int idx = kosq.second * 19 + kosq.first;
        komove[idx] = true;
    }
}

void Network::gather_traindata(std::string filename, TrainVector& data) {
    std::vector<std::string> games = SGFParser::chop_all(filename);
    int gametotal = games.size();
    int gamecount = 0;

    size_t train_pos = 0;
    size_t test_pos = 0;

    myprintf("Total games in file: %d\n", gametotal);
    myprintf("Shuffling...\n");
    std::random_shuffle(games.begin(), games.end());

    while (gamecount < gametotal) {
        std::unique_ptr<SGFTree> sgftree(new SGFTree);

        try {
            sgftree->load_from_string(games[gamecount]);
        } catch (...) {
        };

        size_t movecount = sgftree->count_mainline_moves();
        std::vector<int> tree_moves = sgftree->get_mainline();
        int who_won = sgftree->get_winner();

        SGFTree * treewalk = &(*sgftree);
        size_t counter = 0;

        while (counter < movecount) {
            assert(treewalk != NULL);
            assert(treewalk->get_state() != NULL);
            if (treewalk->get_state()->board.get_boardsize() != 19)
                break;

            if (who_won != FastBoard::BLACK && who_won != FastBoard::WHITE)
                break;

            // check every 3rd move
            //int skip = Random::get_Rng()->randint(10);
            if (/*skip == 0*/1) {
                KoState * state = treewalk->get_state();
                int tomove = state->get_to_move();
                int move;

                if (treewalk->get_child(0) != NULL) {
                    move = treewalk->get_child(0)->get_move(tomove);
                    if (move == SGFTree::EOT) {
                        break;
                    }
                } else {
                    break;
                }

                assert(move == tree_moves[counter]);

                TrainPosition position;

                std::vector<int> moves = state->generate_moves(tomove);
                bool moveseen = false;
                for(auto it = moves.begin(); it != moves.end(); ++it) {
                    if (*it == move) {
                        if (move != FastBoard::PASS) {
                            // get x y coords for actual move
                            std::pair<int, int> xy = state->board.get_xy(move);
                            position.moves[0] = (xy.second * 19) + xy.first;
                        }
                        moveseen = true;
                    }
                }

                bool has_next_moves = counter + 2 < tree_moves.size();
                if (!has_next_moves) {
                    goto skipnext;
                }

                has_next_moves  = tree_moves[counter + 1] != FastBoard::PASS;
                has_next_moves &= tree_moves[counter + 2] != FastBoard::PASS;

                if (!has_next_moves) {
                    goto skipnext;
                }

                if (moveseen && move != FastBoard::PASS && has_next_moves) {
                    position.stm_won = (tomove == who_won);
                    float frac = (float)counter / (float)movecount;
                    position.stm_score = (frac * position.stm_won)
                                          + ((1.0f - frac) * 0.5f);
                    gather_features(state, position.planes);
                    // add next 2 moves to position
                    // we do not check them for legality
                    int next_move = tree_moves[counter + 1];
                    int next_next_move = tree_moves[counter + 2];
                    std::pair<int, int> xy = state->board.get_xy(next_move);
                    position.moves[1] = (xy.second * 19) + xy.first;
                    xy = state->board.get_xy(next_next_move);
                    position.moves[2] = (xy.second * 19) + xy.first;
                    data.push_back(position);
                } else if (move != FastBoard::PASS) {
                    myprintf("Mainline move not found: %d\n", move);
                    goto skipnext;
                }
            }

            counter++;
            treewalk = treewalk->get_child(0);
        }

skipnext:
        gamecount++;
        if (gamecount % 100 == 0) {
            myprintf("Game %d, %d new positions, %d total\n",
                     gamecount, data.size(), train_pos + data.size());
        }
        if (gamecount % 50000 == 0) {
            std::cout << "Shuffling training data...";
            std::random_shuffle(data.begin(), data.end());
            std::cout << "writing: ";
            train_network(data, train_pos, test_pos);
        }
    }

    std::cout << "Shuffling training data...";
    std::random_shuffle(data.begin(), data.end());
    std::cout << "writing: ";
    train_network(data, train_pos, test_pos);

    std::cout << train_pos << " training positions." << std::endl;
    std::cout << test_pos << " testing positions." << std::endl;

    myprintf("Gathering pass done.\n");
}

int Network::rev_rotate_nn_idx(const int vertex, int symmetry) {
    static const int invert[] = {0, 1, 2, 3, 4, 6, 5, 7};
    assert(rotate_nn_idx(rotate_nn_idx(vertex, symmetry), invert[symmetry])
           == vertex);
    return rotate_nn_idx(vertex, invert[symmetry]);
}

int Network::rotate_nn_idx(const int vertex, int symmetry) {
    assert(vertex >= 0 && vertex < 19*19);
    assert(symmetry >= 0 && symmetry < 8);
    int x = vertex % 19;
    int y = vertex / 19;
    int newx;
    int newy;

    if (symmetry >= 4) {
        std::swap(x, y);
        symmetry -= 4;
    }

    if (symmetry == 0) {
        newx = x;
        newy = y;
    } else if (symmetry == 1) {
        newx = x;
        newy = 19 - y - 1;
    } else if (symmetry == 2) {
        newx = 19 - x - 1;
        newy = y;
    } else {
        assert(symmetry == 3);
        newx = 19 - x - 1;
        newy = 19 - y - 1;
    }

    int newvtx = (newy * 19) + newx;
    assert(newvtx >= 0 && newvtx < 19*19);
    return newvtx;
}

void Network::train_network(TrainVector& data,
                            size_t& total_train_pos,
                            size_t& total_test_pos) {
#ifdef USE_CAFFE
    size_t data_size = data.size();
    size_t traincut = (data_size * 96) / 100;

    size_t train_pos = 0;
    size_t test_pos = 0;

    boost::scoped_ptr<caffe::db::DB> train_db(caffe::db::GetDB("leveldb"));
    std::string dbTrainName("leela_train");
    train_db->Open(dbTrainName.c_str(), caffe::db::WRITE);
    boost::scoped_ptr<caffe::db::DB> train_label_db(caffe::db::GetDB("leveldb"));
    std::string dbTrainLabelName("leela_train_label");
    train_label_db->Open(dbTrainLabelName.c_str(), caffe::db::WRITE);

    boost::scoped_ptr<caffe::db::DB> test_db(caffe::db::GetDB("leveldb"));
    std::string dbTestName("leela_test");
    test_db->Open(dbTestName.c_str(), caffe::db::WRITE);
    boost::scoped_ptr<caffe::db::DB> test_label_db(caffe::db::GetDB("leveldb"));
    std::string dbTestLabelName("leela_test_label");
    test_label_db->Open(dbTestLabelName.c_str(), caffe::db::WRITE);

    boost::scoped_ptr<caffe::db::Transaction> train_txn(train_db->NewTransaction());
    boost::scoped_ptr<caffe::db::Transaction> test_txn(test_db->NewTransaction());
    boost::scoped_ptr<caffe::db::Transaction> train_label_txn(train_label_db->NewTransaction());
    boost::scoped_ptr<caffe::db::Transaction> test_label_txn(test_label_db->NewTransaction());

    for (int pass = 0; pass < 1; pass++) {
        size_t data_pos = 0;
        for (auto it = data.begin(); it != data.end(); ++it) {
            TrainPosition& position = *it;
            int move = position.moves[0];
            NNPlanes& nnplanes = position.planes;
            float stm_won = position.stm_won;
            float stm_score = position.stm_score;

            // train data
            caffe::Datum datum;
            size_t datum_channels = nnplanes.size();
            datum.set_channels(datum_channels);
            datum.set_height(19);
            datum.set_width(19);
            std::string buffer(datum_channels * 19 * 19, '\0');
            // check whether to rotate the position
            int symmetry;
            if (pass == 0) {
                symmetry = Random::get_Rng()->randint(8);
            } /*else {
                assert(pass == 1);
                symmetry = Random::get_Rng()->randint(4) + 4;
                }*/
            // set (rotated) bitmaps
            for (size_t p = 0; p < nnplanes.size(); p++) {
                BoardPlane tmp;
                for (size_t b = 0; b < nnplanes[p].size(); b++) {
                    float val = nnplanes[p][b];
                    int rot_idx = rotate_nn_idx((int)b, symmetry);
                    tmp[rot_idx] = val;
                }
                if (p == 0) {
                    assert(tmp[rot_move] == true);
                } else if (p == 1 || p == 2) {
                    assert(tmp[rot_move] == false);
                }
                for (size_t b = 0; b < tmp.size(); b++) {
                    buffer[(p * (19 * 19)) + b] = (int)tmp[b];
                }
            }
            datum.set_data(buffer);
            std::string out;
            datum.SerializeToString(&out);

            // labels
            caffe::Datum datum_label;
            datum_label.set_channels(5);
            datum_label.set_height(1);
            datum_label.set_width(1);

            int this_move = rotate_nn_idx(move, symmetry);
            int next_move = rotate_nn_idx(position.moves[1], symmetry);
            int next_next_move = rotate_nn_idx(position.moves[2], symmetry);

            datum_label.add_float_data((float)this_move);
            datum_label.add_float_data((float)next_move);
            datum_label.add_float_data((float)next_next_move);
            datum_label.add_float_data((float)stm_score);
            datum_label.add_float_data((float)stm_won);
            std::string label_out;
            datum_label.SerializeToString(&label_out);

            data_pos++;
            if (data_pos > traincut) {
                std::stringstream ss;
                ss << test_pos;
                test_pos++;
                test_txn->Put(ss.str(), out);
                test_label_txn->Put(ss.str(), label_out);
                if (test_pos % 10000 == 0) {
                    std::cout << "t";
                    test_txn->Commit();
                    test_label_txn->Commit();
                    test_txn.reset(test_db->NewTransaction());
                    test_label_txn.reset(test_label_db->NewTransaction());
                }
            } else {
                std::stringstream ss;
                ss << train_pos;
                train_pos++;
                train_txn->Put(ss.str(), out);
                train_label_txn->Put(ss.str(), label_out);
                if (train_pos % 10000 == 0) {
                    std::cout << symmetry;
                    train_txn->Commit();
                    train_label_txn->Commit();
                    train_txn.reset(train_db->NewTransaction());
                    train_label_txn.reset(train_label_db->NewTransaction());
                }
            }
        }
    }
    data.clear();

    train_txn->Commit();
    test_txn->Commit();
    train_label_txn->Commit();
    test_label_txn->Commit();

    total_train_pos += train_pos;
    total_test_pos += test_pos;

    std::cout << std::endl;
#endif
}

void Network::autotune_from_file(std::string filename) {
#ifdef USE_CAFFE
    {
        boost::scoped_ptr<caffe::db::DB> train_db(caffe::db::GetDB("leveldb"));
        std::string dbTrainName("leela_train");
        train_db->Open(dbTrainName.c_str(), caffe::db::NEW);
        boost::scoped_ptr<caffe::db::DB> test_db(caffe::db::GetDB("leveldb"));
        std::string dbTestName("leela_test");
        test_db->Open(dbTestName.c_str(), caffe::db::NEW);
    }
#endif
    TrainVector data;
    gather_traindata(filename, data);
}

std::string Network::get_backend() {
#ifdef USE_BLAS
#ifndef __APPLE__
    return std::string("BLAS core: " + std::string(openblas_get_corename()));
#else
    return std::string("BLAS core: Apple Accelerate");
#endif
#elif defined(USE_OPENCL)
    return OpenCL::get_OpenCL()->get_device_name();
#elif defined(USE_CAFFE)
    return std::string("Caffe");
#endif
}
