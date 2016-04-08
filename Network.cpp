#include "config.h"
#ifdef USE_NETS
#include <algorithm>
#include <cassert>
#include <list>
#include <set>
#include <iostream>
#include <fstream>
#include <memory>
#include <boost/utility.hpp>
#include <boost/tr1/array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/format.hpp>

#include <caffe/proto/caffe.pb.h>
#include <caffe/util/db.hpp>
#include <caffe/util/io.hpp>
#include <caffe/blob.hpp>

#include "SGFTree.h"
#include "SGFParser.h"
#include "Utils.h"
#include "FastBoard.h"
#include "Random.h"
#include "Network.h"

using namespace Utils;
using namespace caffe;

Network* Network::s_net = nullptr;

extern std::tr1::array<float, 52800> conv1_w;
extern std::tr1::array<float, 96> conv1_b;
extern std::tr1::array<float, 96> bn1_w1;
extern std::tr1::array<float, 96> bn1_w2;
extern float bn1_w3;
extern std::tr1::array<float, 110592> conv2_w;
extern std::tr1::array<float, 128> conv2_b;
extern std::tr1::array<float, 128> bn2_w1;
extern std::tr1::array<float, 128> bn2_w2;
extern float bn2_w3;
extern std::tr1::array<float, 36864> conv3_w;
extern std::tr1::array<float, 32> conv3_b;
extern std::tr1::array<float, 32> bn3_w1;
extern std::tr1::array<float, 32> bn3_w2;
extern float bn3_w3;
extern std::tr1::array<float, 9216> conv4_w;
extern std::tr1::array<float, 32> conv4_b;
extern std::tr1::array<float, 32> bn4_w1;
extern std::tr1::array<float, 32> bn4_w2;
extern float bn4_w3;
extern std::tr1::array<float, 288> conv5_w;
extern std::tr1::array<float, 1> conv5_b;

Network * Network::get_Network(void) {
    if (!s_net) {
        s_net = new Network();
        s_net->initialize();
    }
    return s_net;
}

void Network::benchmark(FastState * state) {
    static const int BENCH_AMOUNT = 1000;
    Time start;

    for (int loop = 0; loop < BENCH_AMOUNT; loop++) {
        auto vec = get_scored_moves(state);
    }

    Time end;

    myprintf("%d predictions in %5.2f seconds -> %d p/s\n",
             BENCH_AMOUNT,
             (float)Time::timediff(start,end)/100.0,
             (int)((float)BENCH_AMOUNT/((float)Time::timediff(start,end)/100.0)));
}

void Network::initialize(void) {
    myprintf("Initializing DCNN...");
    Caffe::set_mode(Caffe::CPU);

    net.reset(new Net<float>("model_4289.txt", TEST));
    net->CopyTrainedLayersFrom("model_4289.caffemodel");

    myprintf("Inputs: %d Outputs: %d\n",
        net->num_inputs(), net->num_outputs());

    Blob<float>* input_layer = net->input_blobs()[0];
    int num_channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    myprintf("Input: channels=%d, width=%d, height=%d\n", num_channels, width, height);

    Blob<float>* output_layer = net->output_blobs()[0];
    int num_out_channels = output_layer->channels();
    width = output_layer->width();
    height = output_layer->height();
    myprintf("Output: channels=%d, width=%d, height=%d\n", num_out_channels, width, height);

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
            out << blob.blob.shape_string() << std::endl;
            for (int idx = 0; idx < blob.count(); idx++) {
                out << blob.cpu_data()[idx] << ", ";
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
}

template<int filter_size, int channels, int outputs,
         unsigned long W, unsigned long B>
void convolve(std::vector<float>& input,
              std::tr1::array<float, W>& weights,
              std::tr1::array<float, B>& biases,
              std::vector<float>& output) {
    assert(&input != &output);

    // fixed for 19x19
    constexpr int width = 19;
    constexpr int height = 19;

    // size 5 = mid 3
    constexpr int pad = (filter_size / 2) + 1;
    constexpr int extent = pad - 1;

    for (unsigned int o = 0; o < outputs; o++) {
        for (unsigned int oh = 0; oh < height; oh++) {
            for (unsigned int ow = 0; ow < width; ow++) {
                // accumulator
                float val = 0.0f;

                int fwstart = ow - extent;
                int fwend   = ow + extent;
                int fhstart = oh - extent;
                int fhend   = oh + extent;

                if (fwstart >= 0 && fwend < width
                    && fhstart >= 0 && fhend < height) {
                    for (unsigned int c = 0; c < channels; c++) {
                        // reset filter start (e.g 96 22 5 5)
                        unsigned int fidx = (o * channels + c) * filter_size * filter_size;
                        for (unsigned int i = 0; i < filter_size; i++) {
                            for (unsigned int j = 0; j < filter_size; j++) {
                                unsigned int ch = fhstart + i;
                                unsigned int cw = fwstart + j;
                                val +=
                                    input[(c * height + ch) * width + cw]
                                    *
                                    weights[fidx++];
                            }
                        }
                    }
                } else {
                    for (unsigned int c = 0; c < channels; c++) {
                        // reset filter start (e.g 96 22 5 5)
                        unsigned int fidx = (o * channels + c) * filter_size * filter_size;
                        for (int ch = fhstart; ch <= fhend; ch++) {
                            for (int cw = fwstart; cw <= fwend; cw++) {
                                // "zero padding"
                                if (ch < 0 || ch >= height) {
                                    fidx++;
                                    continue;
                                }
                                if (cw < 0 || cw >= width) {
                                    fidx++;
                                    continue;
                                }
                                val +=
                                    input[(c * height + ch) * width + cw]
                                    *
                                    weights[fidx++];
                            }
                        }
                    }
                }

                val += biases[o];

                // ReLU
                val = (val > 0.0f) ? val : 0.0f;
                output[(o * height + oh) * width + ow] = val;
            }
        }
    }
}

template<unsigned long W1>
void batchnorm(std::vector<float>& input,
               int channels,
               std::tr1::array<float, W1>& means,
               std::tr1::array<float, W1>& variances,
               float scale,
               std::vector<float>& output)
{
    // fixed for 19x19
    const int width = 19;
    const int height = 19;

    assert(channels == W1);

    for (int c = 0; c < channels; ++c) {
        float mean = means[c] / scale;
        float variance = variances[c] / scale;
        float stddiv = sqrtf(variance);
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                float val = input[(c * height + h) * width + w];
                val -= mean;
                val /= stddiv;
                output[(c * height + h) * width + w] = val;
            }
        }
    }
}

void softmax(std::vector<float>& input,
             std::vector<float>& output) {
    assert(&input != &output);

    float alpha = *std::max_element(input.begin(), input.end());

    for (size_t i = 0; i < output.size(); i++) {
        float numer = std::exp(input[i] - alpha);
        float denom = 0.0f;
        for (size_t j = 0; j < input.size(); j++) {
            denom += std::exp(input[j] - alpha);
        }
        output[i] = numer / denom;
    }
}

std::vector<std::pair<float, int>> Network::get_scored_moves(FastState * state) {
    std::vector<std::pair<float, int>> result;
    if (state->board.get_boardsize() != 19) {
        return result;
    }

    NNPlanes planes;
    gather_features(state, planes);

#ifdef CAFFE
    Blob<float>* input_layer = net->input_blobs()[0];
    int channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    assert(channels == (int)planes.size());
    assert(width == state->board.get_boardsize());
    assert(height == state->board.get_boardsize());
    float* input_data = input_layer->mutable_cpu_data();
#else
    const int channels = 22;
    const int width = 19;
    const int height = 19;
    const int max_channels = 128;
    std::vector<float> input_data(max_channels * width * height);
    std::vector<float> output_data(max_channels * width * height);
    std::vector<float> softmax_data(width * height);
#endif
    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                input_data[(c * height + h) * width + w] = (float)planes[c][h * 19 + w];
            }
        }
    }
#ifndef CAFFE
    // 96 22 5 5
    convolve<5, 22, 96>(input_data, conv1_w, conv1_b, output_data);
    batchnorm(output_data, 96, bn1_w1, bn1_w2, bn1_w3, input_data);
    // 128 96 3 3
    convolve<3, 96, 128>(input_data, conv2_w, conv2_b, output_data);
    batchnorm(output_data, 128, bn2_w1, bn2_w2, bn2_w3, input_data);
    // 32 128 3 3
    convolve<3, 128, 32>(input_data, conv3_w, conv3_b, output_data);
    batchnorm(output_data, 32, bn3_w1, bn3_w2, bn3_w3, input_data);
    // 32 32 3 3
    convolve<3, 32, 32>(input_data, conv4_w, conv4_b, output_data);
    batchnorm(output_data, 32, bn4_w1, bn4_w2, bn4_w3, input_data);
    // 1 32 3 3
    convolve<3, 32, 1>(input_data, conv5_w, conv5_b, output_data);
    softmax(output_data, softmax_data);

    std::vector<float>& outputs = softmax_data;
#else
    net->Forward();
    Blob<float>* output_layer = net->output_blobs()[0];
    const float* begin = output_layer->cpu_data();
    const float* end = begin + output_layer->channels();
    auto outputs = std::vector<float>(begin, end);
#endif
    int idx = 0;

    std::vector<std::string> display_map;
    std::string line;

    for (auto it = outputs.begin(); it != outputs.end(); ++it, ++idx) {
        float val = *it;
        line += boost::str(boost::format("%3d ") % int(val * 1000));
        if (idx % 19 == 18) {
            display_map.push_back(line);
            line.clear();
        }
        int x = idx % 19;
        int y = idx / 19;
        int vtx = state->board.get_vertex(x, y);
        if (state->board.get_square(vtx) == FastBoard::EMPTY) {
            result.push_back(std::make_pair(val, vtx));
        }
    }
    std::stable_sort(result.rbegin(), result.rend());

    for (int i = display_map.size() - 1; i >= 0; --i) {
        std::cerr << display_map[i] << std::endl;
    }

    float cum = 0.0f;
    size_t tried = 0;
    while (cum < 0.85f && tried < result.size()) {
        if (result[tried].first < 0.01f) break;
        std::cerr << boost::format("%1.3f (") % result[tried].first
                  << state->board.move_to_text(result[tried].second)
                  << ")" << std::endl;
        cum += result[tried].first;
        tried++;
    }

    return result;
}

void Network::gather_features(FastState * state, NNPlanes & planes) {
    planes.resize(22);
    BoardPlane& empt_color = planes[0];
    BoardPlane& move_color = planes[1];
    BoardPlane& othr_color = planes[2];
    BoardPlane& libs_1     = planes[3];
    BoardPlane& libs_2     = planes[4];
    BoardPlane& libs_3     = planes[5];
    BoardPlane& libs_4p    = planes[6];
    BoardPlane& libs_1_e   = planes[7];
    BoardPlane& libs_2_e   = planes[8];
    BoardPlane& libs_3_e   = planes[9];
    BoardPlane& libs_4p_e  = planes[10];
    BoardPlane& lastmoves  = planes[11];
    BoardPlane& after_1    = planes[12];
    BoardPlane& after_2    = planes[13];
    BoardPlane& after_3    = planes[14];
    BoardPlane& after_4p   = planes[15];
    BoardPlane& after_1_e  = planes[16];
    BoardPlane& after_2_e  = planes[17];
    BoardPlane& after_3_e  = planes[18];
    BoardPlane& after_4p_e = planes[19];
    BoardPlane& ladder     = planes[20];
    BoardPlane& komove     = planes[21];

    int tomove = state->get_to_move();
    // collect white, black occupation planes
    for (int j = 0; j < 19; j++) {
        for(int i = 0; i < 19; i++) {
            int vtx = state->board.get_vertex(i, j);
            FastBoard::square_t color =
                state->board.get_square(vtx);
            int idx = j * 19 + i;
            if (color != FastBoard::EMPTY) {
                int rlibs = state->board.count_rliberties(vtx);
                if (rlibs == 1) {
                    if (color == tomove) {
                        libs_1[idx] = true;
                        move_color[idx] = true;
                    } else {
                        libs_1_e[idx] = true;
                        othr_color[idx] = true;
                    }
                } else if (rlibs == 2) {
                    if (color == tomove) {
                        libs_2[idx] = true;
                        move_color[idx] = true;
                    } else {
                        libs_2_e[idx] = true;
                        othr_color[idx] = true;
                    }
                } else if (rlibs == 3) {
                    if (color == tomove) {
                        libs_3[idx] = true;
                        move_color[idx] = true;
                    } else {
                        libs_3_e[idx] = true;
                        othr_color[idx] = true;
                    }
                } else if (rlibs >= 4) {
                    if (color == tomove) {
                        libs_4p[idx] = true;
                        move_color[idx] = true;
                    } else {
                        libs_4p_e[idx] = true;
                        othr_color[idx] = true;
                    }
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
                int ae = state->board.count_pliberties(vtx);
                if (ss > 0 && ae == 2) {
                    int ll = state->board.check_losing_ladder(tomove, vtx);
                    ladder[idx] = ll;
                }
            }
        }
    }

    if (state->get_last_move() > 0) {
        std::pair<int, int> lastmove = state->board.get_xy(state->get_last_move());
        int idx = lastmove.second * 19 + lastmove.first;
        lastmoves[idx] = true;
        if (state->get_prevlast_move() > 0) {
            std::pair<int, int> prevlast = state->board.get_xy(state->get_prevlast_move());
            int idxp = prevlast.second * 19 + prevlast.first;
            lastmoves[idxp] = true;
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

    myprintf("Total games in file: %d\n", gametotal);

    while (gamecount < gametotal) {
        std::unique_ptr<SGFTree> sgftree(new SGFTree);

        try {
            sgftree->load_from_string(games[gamecount]);
        } catch (...) {
        };

        int movecount = sgftree->count_mainline_moves();

        SGFTree * treewalk = &(*sgftree);
        int counter = 0;

        while (counter < movecount) {
            assert(treewalk != NULL);
            assert(treewalk->get_state() != NULL);
            if (treewalk->get_state()->board.get_boardsize() != 19)
                break;

            // check every 3rd move
            //int skip = Random::get_Rng()->randint(3);
            if (1) {
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

                TrainPosition position;

                std::vector<int> moves = state->generate_moves(tomove);
                bool moveseen = false;
                for(auto it = moves.begin(); it != moves.end(); ++it) {
                    if (*it == move) {
                        if (move != FastBoard::PASS) {
                            // get x y coords for actual move
                            std::pair<int, int> xy = state->board.get_xy(move);
                            position.first = (xy.second * 19) + xy.first;
                        }
                        moveseen = true;
                    }
                }

                if (moveseen && move != FastBoard::PASS) {
                    gather_features(state, position.second);
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
            myprintf("Game %d, %3d moves, %d positions\n", gamecount,
                     movecount, data.size());
        }
    }

    myprintf("Gathering pass done.\n");

    std::cout << "Shuffling training data...";
    std::random_shuffle(data.begin(), data.end());
    std::cout << "done." << std::endl;
}

int Network::rotate_nn_idx(int vertex, int symmetry) {
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
    } else if (symmetry == 3) {
        newx = 19 - x - 1;
        newy = 19 - y - 1;
    }

    int newvtx = (newy * 19) + newx;
    assert(newvtx >= 0 && newvtx < 19*19);
    return newvtx;
}

void Network::train_network(TrainVector& data) {
    size_t data_size = data.size();
    size_t traincut = (data_size * 96) / 100;
    size_t data_pos = 0;

    size_t train_pos = 0;
    size_t test_pos = 0;

    boost::scoped_ptr<caffe::db::DB> train_db(caffe::db::GetDB("leveldb"));
    std::string dbTrainName("leela_train");
    train_db->Open(dbTrainName.c_str(), caffe::db::NEW);
    boost::scoped_ptr<caffe::db::Transaction> train_txn(train_db->NewTransaction());

    boost::scoped_ptr<caffe::db::DB> test_db(caffe::db::GetDB("leveldb"));
    std::string dbTestName("leela_test");
    test_db->Open(dbTestName.c_str(), caffe::db::NEW);
    boost::scoped_ptr<caffe::db::Transaction> test_txn(test_db->NewTransaction());

    // Every position in every rotation/symmetry
    for (int symmetry = 0; symmetry < 8; ++symmetry) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            TrainPosition& position = *it;
            int move = position.first;
            NNPlanes& nnplanes = position.second;
            caffe::Datum datum;
            datum.set_channels(22);
            datum.set_height(19);
            datum.set_width(19);
            // store (rotated) move
            datum.set_label(rotate_nn_idx(move, symmetry));
            // stpre (rotated) bitmaps
            for (size_t p = 0; p < nnplanes.size(); p++) {
                for (size_t b = 0; b < nnplanes[p].size(); b++) {
                    int idx = rotate_nn_idx(b, symmetry);
                    datum.add_float_data((float)nnplanes[p][idx]);
                }
            }
            std::string out;
            datum.SerializeToString(&out);

            data_pos++;
            if (data_pos > traincut) {
                std::stringstream ss;
                ss << test_pos;
                test_pos++;
                test_txn->Put(ss.str(), out);
                if (test_pos % 10000 == 0) {
                    std::cout << "t";
                    test_txn->Commit();
                    test_txn.reset(test_db->NewTransaction());
                }
            } else {
                std::stringstream ss;
                ss << train_pos;
                train_pos++;
                train_txn->Put(ss.str(), out);
                if (train_pos % 10000 == 0) {
                    std::cout << symmetry;
                    train_txn->Commit();
                    train_txn.reset(train_db->NewTransaction());
                }
            }
        }
    }
    train_txn->Commit();
    test_txn->Commit();

    std::cout << train_pos << " training positions." << std::endl;
    std::cout << test_pos << " testing positions." << std::endl;
}

void Network::autotune_from_file(std::string filename) {
    TrainVector data;

    gather_traindata(filename, data);

    train_network(data);
}
#endif
