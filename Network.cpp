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
#include <thread>
#include <boost/utility.hpp>
#include <boost/format.hpp>

#ifdef USE_CAFFE
#include <caffe/proto/caffe.pb.h>
#include <caffe/util/db.hpp>
#include <caffe/util/io.hpp>
#include <caffe/blob.hpp>

using namespace caffe;
#endif
#include "Im2Col.h"
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif
#ifdef USE_MKL
#include <mkl.h>
#endif
#ifdef USE_OPENBLAS
#include <cblas.h>
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
#include "GTP.h"
#include "Utils.h"

using namespace Utils;

Network* Network::s_Net = nullptr;
#ifdef USE_CAFFE
std::unique_ptr<caffe::Net> Network::s_net;
#endif

extern std::array<float, 41472> conv1_w;
extern std::array<float, 256> conv1_b;
extern std::array<float, 256> bn1_w1;
extern std::array<float, 256> bn1_w2;
extern std::array<float, 589824> conv2_w;
extern std::array<float, 256> conv2_b;
extern std::array<float, 256> bn2_w1;
extern std::array<float, 256> bn2_w2;
extern std::array<float, 589824> conv3_w;
extern std::array<float, 256> conv3_b;
extern std::array<float, 256> bn3_w1;
extern std::array<float, 256> bn3_w2;
extern std::array<float, 589824> conv4_w;
extern std::array<float, 256> conv4_b;
extern std::array<float, 256> bn4_w1;
extern std::array<float, 256> bn4_w2;
extern std::array<float, 589824> conv5_w;
extern std::array<float, 256> conv5_b;
extern std::array<float, 256> bn5_w1;
extern std::array<float, 256> bn5_w2;
extern std::array<float, 589824> conv6_w;
extern std::array<float, 256> conv6_b;
extern std::array<float, 256> bn6_w1;
extern std::array<float, 256> bn6_w2;
extern std::array<float, 589824> conv7_w;
extern std::array<float, 256> conv7_b;
extern std::array<float, 256> bn7_w1;
extern std::array<float, 256> bn7_w2;
extern std::array<float, 589824> conv8_w;
extern std::array<float, 256> conv8_b;
extern std::array<float, 256> bn8_w1;
extern std::array<float, 256> bn8_w2;
extern std::array<float, 589824> conv9_w;
extern std::array<float, 256> conv9_b;
extern std::array<float, 256> bn9_w1;
extern std::array<float, 256> bn9_w2;
extern std::array<float, 589824> conv10_w;
extern std::array<float, 256> conv10_b;
extern std::array<float, 256> bn10_w1;
extern std::array<float, 256> bn10_w2;
extern std::array<float, 589824> conv11_w;
extern std::array<float, 256> conv11_b;
extern std::array<float, 256> bn11_w1;
extern std::array<float, 256> bn11_w2;
extern std::array<float, 589824> conv12_w;
extern std::array<float, 256> conv12_b;
extern std::array<float, 256> bn12_w1;
extern std::array<float, 256> bn12_w2;
extern std::array<float, 589824> conv13_w;
extern std::array<float, 256> conv13_b;
extern std::array<float, 256> bn13_w1;
extern std::array<float, 256> bn13_w2;
extern std::array<float, 589824> conv14_w;
extern std::array<float, 256> conv14_b;
extern std::array<float, 256> bn14_w1;
extern std::array<float, 256> bn14_w2;
extern std::array<float, 589824> conv15_w;
extern std::array<float, 256> conv15_b;
extern std::array<float, 256> bn15_w1;
extern std::array<float, 256> bn15_w2;
extern std::array<float, 589824> conv16_w;
extern std::array<float, 256> conv16_b;
extern std::array<float, 256> bn16_w1;
extern std::array<float, 256> bn16_w2;
extern std::array<float, 589824> conv17_w;
extern std::array<float, 256> conv17_b;
extern std::array<float, 256> bn17_w1;
extern std::array<float, 256> bn17_w2;
extern std::array<float, 589824> conv18_w;
extern std::array<float, 256> conv18_b;
extern std::array<float, 256> bn18_w1;
extern std::array<float, 256> bn18_w2;
extern std::array<float, 589824> conv19_w;
extern std::array<float, 256> conv19_b;
extern std::array<float, 256> bn19_w1;
extern std::array<float, 256> bn19_w2;
extern std::array<float, 589824> conv20_w;
extern std::array<float, 256> conv20_b;
extern std::array<float, 256> bn20_w1;
extern std::array<float, 256> bn20_w2;
extern std::array<float, 589824> conv21_w;
extern std::array<float, 256> conv21_b;
extern std::array<float, 256> bn21_w1;
extern std::array<float, 256> bn21_w2;
extern std::array<float, 589824> conv22_w;
extern std::array<float, 256> conv22_b;
extern std::array<float, 256> bn22_w1;
extern std::array<float, 256> bn22_w2;
extern std::array<float, 589824> conv23_w;
extern std::array<float, 256> conv23_b;
extern std::array<float, 256> bn23_w1;
extern std::array<float, 256> bn23_w2;
extern std::array<float, 589824> conv24_w;
extern std::array<float, 256> conv24_b;
extern std::array<float, 256> bn24_w1;
extern std::array<float, 256> bn24_w2;
extern std::array<float, 589824> conv25_w;
extern std::array<float, 256> conv25_b;
extern std::array<float, 256> bn25_w1;
extern std::array<float, 256> bn25_w2;

extern std::array<float, 512> conv26_w;
extern std::array<float, 2> conv26_b;
extern std::array<float, 2> bn26_w1;
extern std::array<float, 2> bn26_w2;

extern std::array<float, 261364> ip27_w;
extern std::array<float, 362> ip27_b;

extern std::array<float, 256> conv28_w;
extern std::array<float, 1> conv28_b;
extern std::array<float, 1> bn28_w1;
extern std::array<float, 1> bn28_w2;

extern std::array<float, 92416> ip29_w;
extern std::array<float, 256> ip29_b;

extern std::array<float, 256> ip30_w;
extern std::array<float, 1> ip30_b;

Network * Network::get_Network(void) {
    if (!s_Net) {
        s_Net = new Network();
        s_Net->initialize();
    }
    return s_Net;
}

void Network::benchmark(GameState * state) {
    {
        int BENCH_AMOUNT = 1600;
        int cpus = cfg_num_threads;
        int iters_per_thread = (BENCH_AMOUNT + (cpus - 1)) / cpus;

        Time start;

        ThreadGroup tg(thread_pool);
        for (int i = 0; i < cpus; i++) {
            tg.add_task([iters_per_thread, state]() {
                GameState mystate = *state;
                for (int loop = 0; loop < iters_per_thread; loop++) {
                    auto vec = get_scored_moves(&mystate, Ensemble::RANDOM_ROTATION);
                }
            });
        };
        tg.wait_all();

        Time end;

        myprintf("%5d evaluations in %5.2f seconds -> %d n/s\n",
                 BENCH_AMOUNT,
                 (float)Time::timediff(start,end)/100.0,
                 (int)((float)BENCH_AMOUNT/((float)Time::timediff(start,end)/100.0)));
    }
}

void Network::initialize(void) {
#ifdef USE_OPENCL
    myprintf("Initializing OpenCL\n");
    opencl.initialize();
    myprintf("Transferring weights to GPU...");
    // input
    opencl_net.push_convolve(3, conv1_w, conv1_b);
    opencl_net.push_batchnorm(361, bn1_w1, bn1_w2);
    // residual blocks
    opencl_net.push_residual(3, conv2_w, conv2_b, bn2_w1, bn2_w2,
                                conv3_w, conv3_b, bn3_w1, bn3_w2);
    opencl_net.push_residual(3, conv4_w, conv4_b, bn4_w1, bn4_w2,
                                conv5_w, conv5_b, bn5_w1, bn5_w2);
    opencl_net.push_residual(3, conv6_w, conv6_b, bn6_w1, bn6_w2,
                                conv7_w, conv7_b, bn7_w1, bn7_w2);
    opencl_net.push_residual(3, conv8_w, conv8_b, bn8_w1, bn8_w2,
                                conv9_w, conv9_b, bn9_w1, bn9_w2);
    opencl_net.push_residual(3, conv10_w, conv10_b, bn10_w1, bn10_w2,
                                conv11_w, conv11_b, bn11_w1, bn11_w2);
    opencl_net.push_residual(3, conv12_w, conv12_b, bn12_w1, bn12_w2,
                                conv13_w, conv13_b, bn13_w1, bn13_w2);
    opencl_net.push_residual(3, conv14_w, conv14_b, bn14_w1, bn14_w2,
                                conv15_w, conv15_b, bn15_w1, bn15_w2);
    opencl_net.push_residual(3, conv16_w, conv16_b, bn16_w1, bn16_w2,
                                conv17_w, conv17_b, bn17_w1, bn17_w2);
    opencl_net.push_residual(3, conv18_w, conv18_b, bn18_w1, bn18_w2,
                                conv19_w, conv19_b, bn19_w1, bn19_w2);
    opencl_net.push_residual(3, conv20_w, conv20_b, bn20_w1, bn20_w2,
                                conv21_w, conv21_b, bn21_w1, bn21_w2);
    opencl_net.push_residual(3, conv22_w, conv22_b, bn22_w1, bn22_w2,
                                conv23_w, conv23_b, bn23_w1, bn23_w2);
    opencl_net.push_residual(3, conv24_w, conv24_b, bn24_w1, bn24_w2,
                                conv25_w, conv25_b, bn25_w1, bn25_w2);
    myprintf("done\n");
#endif
#ifdef USE_BLAS
#ifndef __APPLE__
#ifdef USE_OPENBLAS
    openblas_set_num_threads(1);
    myprintf("BLAS Core: %s\n", openblas_get_corename());
#endif
#ifdef USE_MKL
    //mkl_set_threading_layer(MKL_THREADING_SEQUENTIAL);
    mkl_set_num_threads(1);
    MKLVersion Version;
    mkl_get_version(&Version);
    myprintf("BLAS core: MKL %s\n", Version.Processor);
#endif
#endif
#endif
#ifdef USE_CAFFE
    myprintf("Initializing DCNN...");
    Caffe::set_mode(Caffe::GPU);

    s_net.reset(new Net("model_zero.txt", TEST));
    s_net->CopyTrainedLayersFrom("model_zero.caffemodel");

    myprintf("Inputs: %d Outputs: %d\n",
        s_net->num_inputs(), s_net->num_outputs());

    Blob* input_layer = s_net->input_blobs()[0];
    int num_channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    myprintf("Input: channels=%d, width=%d, height=%d\n", num_channels, width, height);

    for (int i = 0; i < s_net->num_outputs(); i++) {
        Blob* output_layer = s_net->output_blobs()[i];
        int num_out_channels = output_layer->channels();
        width = output_layer->width();
        height = output_layer->height();
        myprintf("Output: channels=%d, width=%d, height=%d\n", num_out_channels, width, height);
    }

//#define WRITE_WEIGHTS
#ifdef WRITE_WEIGHTS
    std::ofstream out("weights.txt");
    out << "#include <array>" << std::endl << std::endl;
#endif

    int total_weights = 0;
    auto & layers = s_net->layers();
    myprintf("%d layers:\n", layers.size());
    int layer_num = 1;
#ifdef WRITE_WEIGHTS
    int conv_count = 0;
#endif
    for (auto it = layers.begin(); it != layers.end(); ++it, ++layer_num) {
        myprintf("layer %d (%s)", layer_num, (*it)->type());
        auto & blobs = (*it)->blobs();
        if (blobs.size() > 0) myprintf(" = ");
        for (auto pars = begin(blobs); pars != end(blobs); ++pars) {
            auto const& blob = *(*pars);
            total_weights += blob.count();
            myprintf("%s ", blob.shape_string().c_str());
            if (boost::next(pars) != blobs.end()) myprintf("+ ");
#ifdef WRITE_WEIGHTS
            out << "// " << blob.shape_string() << std::endl;
            if (strcmp((*it)->type(), "Convolution") == 0) {
                if (pars == blobs.begin()) {
                    conv_count++;
                    out << "std::array<float, " << blob.count()
                        << "> conv" << conv_count << "_w = {{" << std::endl;
                } else {
                    out << "std::array<float, " << blob.count()
                        << "> conv" << conv_count << "_b = {{" << std::endl;
                }
            } else if (strcmp((*it)->type(), "BatchNorm") == 0) {
                out << "std::array<float, " << blob.count()
                    << "> bn" << conv_count << "_w" << (pars - blobs.begin()) + 1
                    << " = {{" << std::endl;
            } else if (strcmp((*it)->type(), "InnerProduct") == 0) {
                if (pars == blobs.begin()) {
                    conv_count++;
                    out << "std::array<float, " << blob.count()
                        << "> ip" << conv_count << "_w = {{" << std::endl;
                } else {
                    out << "std::array<float, " << blob.count()
                        << "> ip" << conv_count << "_b = {{" << std::endl;
                }
            } else {
                out << "std::array<float, " << blob.count()
                    << "> sc" << conv_count << "_w" << (pars - blobs.begin()) + 1
                    << " = {{" << std::endl;
            }
            for (int idx = 0; idx < blob.count(); idx++) {
                out << blob.cpu_data<float>()[idx];
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
         size_t W, size_t B>
void convolve(const std::vector<float>& input,
              const std::array<float, W>& weights,
              const std::array<float, B>& biases,
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

    for (unsigned int o = 0; o < outputs; o++) {
        for (unsigned int b = 0; b < spatial_out; b++) {
            output[(o * spatial_out) + b] =
                biases[o] + output[(o * spatial_out) + b];
        }
    }
}

template<unsigned int inputs,
         unsigned int outputs,
         size_t W, size_t B>
void innerproduct(const std::vector<float>& input,
                  const std::array<float, W>& weights,
                  const std::array<float, B>& biases,
                  std::vector<float>& output) {
    assert(B == outputs);

    cblas_sgemv(CblasRowMajor, CblasNoTrans,
                // M     K
                outputs, inputs,
                1.0f, &weights[0], inputs,
                &input[0], 1,
                0.0f, &output[0], 1);

    auto lambda_ReLU = [](float val) { return (val > 0.0f) ?
                                       val : 0.0f; };

    for (unsigned int o = 0; o < outputs; o++) {
        float val = biases[o] + output[o];
        if (outputs == 256) {
            val = lambda_ReLU(val);
        }
        output[o] = val;
    }
}

template<unsigned int channels,
         unsigned int spatial_size>
void batchnorm(const std::vector<float>& input,
               const std::array<float, channels>& means,
               const std::array<float, channels>& variances,
               std::vector<float>& output)
{
    constexpr float epsilon = 1e-5f;

    auto lambda_ReLU = [](float val) { return (val > 0.0f) ?
                                       val : 0.0f; };

    for (unsigned int c = 0; c < channels; ++c) {
        float mean = means[c];
        float variance = variances[c] + epsilon;
        float scale_stddiv = 1.0f / std::sqrt(variance);

        float * out = &output[c * spatial_size];
        float const * in  = &input[c * spatial_size];
        for (unsigned int b = 0; b < spatial_size; b++) {
            out[b] = lambda_ReLU(scale_stddiv * (in[b] - mean));
        }
    }
}
#endif

void Network::softmax(const std::vector<float>& input,
                      std::vector<float>& output,
                      float temperature) {
    assert(&input != &output);

    float alpha = *std::max_element(input.begin(),
                                    input.begin() + output.size());
    alpha /= temperature;

    float denom = 0.0f;
    std::vector<float> helper(output.size());
    for (size_t i = 0; i < output.size(); i++) {
        float val  = std::exp((input[i]/temperature) - alpha);
        helper[i]  = val;
        denom     += val;
    }
    for (size_t i = 0; i < output.size(); i++) {
        output[i] = helper[i] / denom;
    }
}

Network::Netresult Network::get_scored_moves(
    GameState * state, Ensemble ensemble, int rotation) {
    Netresult result;
    if (state->board.get_boardsize() != 19) {
        return result;
    }

    NNPlanes planes;
    gather_features(state, planes);

    if (ensemble == DIRECT) {
        assert(rotation >= 0 && rotation <= 7);
        result = get_scored_moves_internal(state, planes, rotation);
    } else if (ensemble == RANDOM_ROTATION) {
        assert(rotation == -1);
        int rand_rot = Random::get_Rng()->randfix<8>();
        result = get_scored_moves_internal(state, planes, rand_rot);
    } else {
        assert(ensemble == AVERAGE_ALL);
        result = get_scored_moves_internal(state, planes, 0);
        for (int r = 1; r < 8; r++) {
            auto sum_res = get_scored_moves_internal(state, planes, r);
            for (size_t i = 0; i < sum_res.first.size(); i++) {
                assert(result.first[i].second == sum_res.first[i].second);
                result.first[i].first += sum_res.first[i].first;
            }
            result.second += sum_res.second;
        }
        std::for_each(begin(result.first), end(result.first),
                      [](scored_node & sn){ sn.first /= 8.0f; });
        result.second /= 8.0f;
    }

    // if (ensemble == AVERAGE_ALL || ensemble == DIRECT) {
    //     show_heatmap(state, result, true);
    // }

    return result;
}

Network::Netresult Network::get_scored_moves_internal(
    GameState * state, NNPlanes & planes, int rotation) {
    assert(rotation >= 0 && rotation <= 7);
#ifdef USE_CAFFE
    Blob* input_layer = s_net->input_blobs()[0];
    int channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    assert(channels == (int)planes.size());
    assert(width == state->board.get_boardsize());
    assert(height == state->board.get_boardsize());
    float* input_data = input_layer->mutable_cpu_data<float>();
#else
    constexpr int channels = INPUT_CHANNELS;
    assert(channels == planes.size());
    constexpr int width = 19;
    constexpr int height = 19;
    constexpr int max_channels = MAX_CHANNELS;
    std::vector<float> input_data(max_channels * width * height);
    std::vector<float> output_data(max_channels * width * height);
    std::vector<float> policy_data_1(2 * width * height);
    std::vector<float> policy_data_2(2 * width * height);
    std::vector<float> value_data_1(1 * width * height);
    std::vector<float> value_data_2(1 * width * height);
    std::vector<float> policy_out((width * height) + 1);
    std::vector<float> softmax_data((width * height) + 1);
    std::vector<float> winrate_data(256);
    std::vector<float> winrate_out(1);
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
#ifdef USE_OPENCL
    opencl_net.forward(input_data, output_data);
    // Get the moves
    convolve<1, 256, 2>(output_data, conv26_w, conv26_b, policy_data_1);
    batchnorm<2, 361>(policy_data_1, bn26_w1, bn26_w2, policy_data_2);
    innerproduct<2*361, 362>(policy_data_2, ip27_w, ip27_b, policy_out);
    softmax(policy_out, softmax_data, cfg_softmax_temp);
    std::vector<float>& outputs = softmax_data;

    // Now get the score
    convolve<1, 256, 1>(output_data, conv28_w, conv28_b, value_data_1);
    batchnorm<1, 361>(value_data_1, bn28_w1, bn28_w2, value_data_2);
    innerproduct<361, 256>(value_data_2, ip29_w, ip29_b, winrate_data);
    innerproduct<256, 1>(winrate_data, ip30_w, ip30_b, winrate_out);

    // Sigmoid
    float winrate_sig = (1.0f + std::tanh(winrate_out[0])) / 2.0f;
#elif defined(USE_BLAS) && !defined(USE_OPENCL)
    convolve<5,  32,  96>(input_data, conv1_w, conv1_b, output_data);
    std::swap(input_data, output_data);
    softmax(output_data, softmax_data, cfg_softmax_temp);
    // Move scores
    std::vector<float>& outputs = softmax_data;
#endif
#ifdef USE_CAFFE
    s_net->Forward();
    Blob* output_layer_pol = s_net->output_blobs()[0];
    const float* begin = output_layer_pol->cpu_data<float>();
    const float* end = begin + output_layer_pol->channels();
    auto outputs = std::vector<float>(begin, end);
    Blob* output_layer_val = s_net->output_blobs()[1];
    auto output_winrate = *(output_layer_val->cpu_data<float>());
    auto winrate_sig = (1.0f + output_winrate) / 2.0f;
#endif
    std::vector<scored_node> result;
    for (size_t idx = 0; idx < outputs.size(); idx++) {
        if (idx < 19*19) {
            auto rot_idx = rev_rotate_nn_idx(idx, rotation);
            auto val = outputs[rot_idx];
            int x = idx % 19;
            int y = idx / 19;
            int vtx = state->board.get_vertex(x, y);
            if (state->board.get_square(vtx) == FastBoard::EMPTY) {
                result.emplace_back(val, vtx);
            }
        } else {
            result.emplace_back(outputs[idx], FastBoard::PASS);
        }
    }

    return std::make_pair(result, winrate_sig);
}

void Network::show_heatmap(FastState * state, Netresult& result, bool topmoves) {
    auto moves = result.first;
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

    for (auto i = display_map.size() - 1; i >= 0; --i) {
        myprintf("%s\n", display_map[i].c_str());
    }
    assert(result.first.back().second == FastBoard::PASS);
    int pass_score = int(result.first.back().first * 1000);
    myprintf("pass: %d\n", pass_score);
    myprintf("winrate: %f\n", result.second);

    if (topmoves) {
        std::stable_sort(moves.rbegin(), moves.rend());

        float cum = 0.0f;
        size_t tried = 0;
        while (cum < 0.85f && tried < moves.size()) {
            if (moves[tried].first < 0.01f) break;
            myprintf("%1.3f (%s)\n",
                    moves[tried].first,
                    state->board.move_to_text(moves[tried].second).c_str());
            cum += moves[tried].first;
            tried++;
        }
    }
}

void Network::gather_features(GameState * state, NNPlanes & planes) {
    planes.resize(18);
    const size_t our_offset   = 0;
    const size_t their_offset = 8;
    BoardPlane& black_to_move  = planes[16];
    BoardPlane& white_to_move  = planes[17];

    bool whites_move = state->get_to_move() == FastBoard::WHITE;
    if (whites_move) {
        white_to_move.set();
    } else {
        black_to_move.set();
    }

    // Go back in time, fill history boards
    size_t backtracks = 0;
    for (int h = 0; h < 8; h++) {
        int tomove = state->get_to_move();
        // collect white, black occupation planes
        for (int j = 0; j < 19; j++) {
            for(int i = 0; i < 19; i++) {
                int vtx = state->board.get_vertex(i, j);
                FastBoard::square_t color =
                    state->board.get_square(vtx);
                int idx = j * 19 + i;
                if (color != FastBoard::EMPTY) {
                    if (color == tomove) {
                        planes[our_offset + h][idx] = true;
                    } else {
                        planes[their_offset + h][idx] = true;
                    }
                }
            }
        }
        if (!state->undo_move()) {
            break;
        } else {
            backtracks++;
        }
    }

    // Now go back to present day
    for (size_t h = 0; h < backtracks; h++) {
        state->forward_move();
    }
}

void Network::gather_traindata(std::string filename, TrainVector& data) {
    auto games = SGFParser::chop_all(filename);
    auto gametotal = games.size();
    auto gamecount = size_t{0};

    size_t train_pos = 0;
    size_t test_pos = 0;

    myprintf("Total games in file: %d\n", gametotal);

    while (gamecount < 8 * gametotal) {
        auto sgftree = std::make_unique<SGFTree>();
        auto rand_idx = Random::get_Rng()->randuint32(gametotal);
        try {
            sgftree->load_from_string(games[rand_idx]);
        } catch (...) {
        };

        auto treewalk = sgftree.get();
        size_t counter = 0;

        auto tree_moves = sgftree->get_mainline();
        auto state = std::make_unique<GameState>(sgftree->follow_mainline_state());
        state->rewind();

        int who_won = sgftree->get_winner();
        // Accept all komis and handicaps
        // But reject no usable result
        if (who_won != FastBoard::BLACK && who_won != FastBoard::WHITE) {
            goto skipnext;
        }
        if (state->board.get_boardsize() != 19) {
            goto skipnext;
        }

        do {
            int tomove = state->get_to_move();
            int move;

            if (treewalk->get_child(0) != nullptr) {
                move = treewalk->get_child(0)->get_move(tomove);
                if (move == SGFTree::EOT) {
                    break;
                }
            } else {
                break;
            }

            assert(move == tree_moves[counter]);
            int this_move = -1;

            auto moves = state->generate_moves(tomove);
            bool moveseen = false;
            for(auto & gen_move : moves) {
                if (gen_move == move) {
                    if (move != FastBoard::PASS) {
                        // get x y coords for actual move
                        std::pair<int, int> xy = state->board.get_xy(move);
                        this_move = (xy.second * 19) + xy.first;
                    } else {
                        this_move = FastBoard::PASS;
                    }
                    moveseen = true;
                }
            }

            int skip = Random::get_Rng()->randfix<8>();
            if (skip == 0) {
                if (moveseen) {
                    TrainPosition position;
                    position.stm_won_tanh = (tomove == who_won ? 1.0f : -1.0f);
                    position.move = this_move;
                    gather_features(state.get(), position.planes);
                    data.emplace_back(position);
                } else {
                    myprintf("Mainline move not found: %d\n", move);
                    goto skipnext;
                }
            }

            counter++;
            treewalk = treewalk->get_child(0);
        } while (state->forward_move());

skipnext:
        gamecount++;
        if (gamecount % (8*100) == 0) {
            myprintf("Game %d, %d new positions, %d total\n",
                     gamecount/16, data.size(), train_pos + data.size());
        }
        if (gamecount % (250000) == 0) {
            train_network(data, train_pos, test_pos);
        }
    }

    train_network(data, train_pos, test_pos);

    std::cout << train_pos << " training positions." << std::endl;
    std::cout << test_pos << " testing positions." << std::endl;

    myprintf("Gathering pass done.\n");
    exit(0);
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
    size_t traincut = (data_size * 98) / 100;

    size_t train_pos = 0;
    size_t test_pos = 0;

    std::unique_ptr<caffe::db::DB> train_db(caffe::db::GetDB("leveldb"));
    std::string dbTrainName("leela_train");
    train_db->Open(dbTrainName.c_str(), caffe::db::WRITE);
    std::unique_ptr<caffe::db::DB> train_label_db(caffe::db::GetDB("leveldb"));
    std::string dbTrainLabelName("leela_train_label");
    train_label_db->Open(dbTrainLabelName.c_str(), caffe::db::WRITE);

    std::unique_ptr<caffe::db::DB> test_db(caffe::db::GetDB("leveldb"));
    std::string dbTestName("leela_test");
    test_db->Open(dbTestName.c_str(), caffe::db::WRITE);
    std::unique_ptr<caffe::db::DB> test_label_db(caffe::db::GetDB("leveldb"));
    std::string dbTestLabelName("leela_test_label");
    test_label_db->Open(dbTestLabelName.c_str(), caffe::db::WRITE);

    std::unique_ptr<caffe::db::Transaction> train_txn(train_db->NewTransaction());
    std::unique_ptr<caffe::db::Transaction> test_txn(test_db->NewTransaction());
    std::unique_ptr<caffe::db::Transaction> train_label_txn(train_label_db->NewTransaction());
    std::unique_ptr<caffe::db::Transaction> test_label_txn(test_label_db->NewTransaction());

    std::cout << "Shuffling training data...";
    std::random_shuffle(begin(data), end(data));
    std::cout << "writing: ";

    size_t data_pos = 0;
    for (auto & position : data) {
        NNPlanes& nnplanes = position.planes;

        // train data
        caffe::Datum datum;
        size_t datum_channels = nnplanes.size();
        datum.set_channels(datum_channels);
        datum.set_height(19);
        datum.set_width(19);
        std::string buffer(datum_channels * 19 * 19, '\0');
        // check whether to rotate the position
        int symmetry = Random::get_Rng()->randfix<8>();
        for (size_t p = 0; p < nnplanes.size(); p++) {
            BoardPlane tmp;
            for (size_t b = 0; b < nnplanes[p].size(); b++) {
                float val = nnplanes[p][b];
                int rot_idx = rotate_nn_idx((int)b, symmetry);
                tmp[rot_idx] = val;
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
        datum_label.set_channels(2);
        datum_label.set_height(1);
        datum_label.set_width(1);

        int this_move;
        if (position.move != FastBoard::PASS) {
            this_move = rotate_nn_idx(position.move, symmetry);
        } else {
            this_move = 19 * 19;
        }

        datum_label.add_float_data(this_move);
        datum_label.add_float_data((float)position.stm_won_tanh);
        std::string label_out;
        datum_label.SerializeToString(&label_out);

        data_pos++;
        if (data_pos > traincut) {
            std::stringstream ss;
            ss << (total_test_pos + test_pos);
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
            ss << (total_train_pos + train_pos);
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
    TrainVector data;
    gather_traindata(filename, data);
}

std::string Network::get_backend() {
#if defined(USE_OPENCL)
    return opencl.get_device_name();
#elif defined(USE_CAFFE)
    return std::string("Caffe");
#else
#ifdef USE_BLAS
#ifndef __APPLE__
#ifdef USE_OPENBLAS
    return std::string("BLAS core: " + std::string(openblas_get_corename()));
#endif
#ifdef USE_MKL
    MKLVersion Version;
    mkl_get_version(&Version);
    return std::string("BLAS core: " + std::string(Version.Processor));
#endif
#else
    return std::string("BLAS core: Apple Accelerate");
#endif
#endif
#endif
    return std::string("No BLAS backend active");
}
