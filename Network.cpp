#include "config.h"
#ifdef USE_NETS
#include <algorithm>
#include <cassert>
#include <list>
#include <set>
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

#include "tiny_cnn/tiny_cnn.h"

#include "SGFTree.h"
#include "SGFParser.h"
#include "Utils.h"
#include "FastBoard.h"
#include "Random.h"
#include "Network.h"

using namespace Utils;
using namespace caffe;

Network* Network::s_net = nullptr;

Network * Network::get_Network(void) {
    if (!s_net) {
        s_net = new Network();
        s_net->initialize();
    }
    return s_net;
}

void Network::initialize(void) {
    myprintf("Initializing DCNN...");
    Caffe::set_mode(Caffe::CPU);

    net.reset(new Net<float>("model_4258.txt", TEST));
    net->CopyTrainedLayersFrom("model_4258.caffemodel");

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

    int total_weights = 0;
    auto & layers = net->layers();
    myprintf("%d layers:\n", layers.size());
    int layer_num = 1;
    for (auto it = layers.begin(); it != layers.end(); ++it, ++layer_num) {
        myprintf("layer %d (%s)", layer_num, (*it)->type());
        auto & params = (*it)->blobs();
        if (params.size() > 0) myprintf(" = ");
        for (auto pars = params.begin(); pars != params.end(); ++pars) {
            total_weights += (*pars)->count();
            myprintf("%s ", (*pars)->shape_string().c_str());
            if (boost::next(pars) != params.end()) myprintf("+ ");
        }
        myprintf("\n");
    }
    myprintf("%d total DCNN weights\n", total_weights);
}

std::vector<std::pair<float, int>> Network::get_scored_moves(FastState * state) {
    std::vector<std::pair<float, int>> result;
    if (state->board.get_boardsize() != 19) {
        return result;
    }

    NNPlanes planes;
    gather_features(state, planes, false);

    Blob<float>* input_layer = net->input_blobs()[0];
    int channels = input_layer->channels();
    int width = input_layer->width();
    int height = input_layer->height();
    assert(channels == (int)planes.size());
    assert(width == state->board.get_boardsize());
    assert(height == state->board.get_boardsize());
    float* input_data = input_layer->mutable_cpu_data();

    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                input_data[input_layer->offset(0, c, h, w)] = (float)planes[c][h * 19 + w];
            }
        }
    }

    net->Forward();

    Blob<float>* output_layer = net->output_blobs()[0];
    const float* begin = output_layer->cpu_data();
    const float* end = begin + output_layer->channels();
    auto outputs = std::vector<float>(begin, end);
    float maxout = 0.0f;
    int maxvtx = -1;
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
        if (val > maxout) {
            maxout = val;
            maxvtx = vtx;
        }
        result.push_back(std::make_pair(val, vtx));
    }
    std::stable_sort(result.rbegin(), result.rend());

    for (int i = display_map.size() - 1; i >= 0; --i) {
        std::cout << display_map[i] << std::endl;
    }

    float cum = 0.0f;
    size_t tried = 0;
    while (cum < 0.95f && tried < result.size()) {
        if (result[tried].first < 0.01f) break;
        std::cout << boost::format("%1.3f%% (") % result[tried].first
                  << state->board.move_to_text(result[tried].second)
                  << ")" << std::endl;
        cum += result[tried].first;
        tried++;
    }

    std::string move = state->board.move_to_text(maxvtx);
    myprintf("move: %s max index: %d value: %f\n", move.c_str(), maxvtx, maxout);

    return result;
}

void Network::gather_features(FastState * state, NNPlanes & planes, bool rotate) {
    planes.resize(22);
    BoardPlane& empt_color = planes[0];
    BoardPlane& move_color = planes[1];
    BoardPlane& othr_color = planes[2];
    BoardPlane& lastmoves  = planes[3];
    BoardPlane& libs_1     = planes[4];
    BoardPlane& libs_2     = planes[5];
    BoardPlane& libs_3     = planes[6];
    BoardPlane& libs_4p    = planes[7];
    BoardPlane& libs_1_e   = planes[8];
    BoardPlane& libs_2_e   = planes[9];
    BoardPlane& libs_3_e   = planes[10];
    BoardPlane& libs_4p_e  = planes[11];
    BoardPlane& after_1    = planes[12];
    BoardPlane& after_2    = planes[13];
    BoardPlane& after_3    = planes[14];
    BoardPlane& after_4p   = planes[15];
    BoardPlane& after_1_e  = planes[16];
    BoardPlane& after_2_e  = planes[17];
    BoardPlane& after_3_e  = planes[18];
    BoardPlane& after_4p_e = planes[19];
    BoardPlane& komove     = planes[20];
    BoardPlane& ladder     = planes[21];

    // Every position in a random rotation/symmetry
    int symmetry = Random::get_Rng()->randint(8);

    int tomove = state->get_to_move();
    // collect white, black occupation planes
    for (int j = 0; j < 19; j++) {
        for(int i = 0; i < 19; i++) {
            int vtx = state->board.get_vertex(i, j);
            if (rotate) {
                vtx = state->board.rotate_vertex(vtx, symmetry);
            }
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
                    gather_features(state, position.second, true);
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

void Network::train_network(TrainVector& data) {
    nn << convolutional_layer<relu>   (19, 19, 5, 10,  8, padding::same)
       << convolutional_layer<relu>   (19, 19, 3,  8,  8, padding::same)
       << convolutional_layer<relu>   (19, 19, 3,  8,  8, padding::same)
       << convolutional_layer<softmax>(19, 19, 3,  8,  1, padding::same);

    size_t total_weights = 0;
    for (size_t i = 0; i < nn.depth(); i++) {
        vec_t& weight = nn[i]->weight();
        vec_t& bias = nn[i]->bias();
        std::cout << "#layer:" << i << "\n";
        std::cout << "layer type:" << nn[i]->layer_type() << "\n";
        std::cout << "input:" << nn[i]->in_size() << "(" << nn[i]->in_shape() << ")\n";
        std::cout << "output:" << nn[i]->out_size() << "(" << nn[i]->out_shape() << ")\n";
        std::cout << "weights: " << weight.size() << " bias: " << bias.size() << std::endl;
        total_weights += weight.size();
    }
    std::cout << total_weights << " weights to train." << std::endl;

    std::vector<vec_t> train_data;
    std::vector<vec_t> test_data;
    std::vector<label_t> train_label;
    std::vector<label_t> test_label;

    size_t data_size = data.size();
    size_t traincut = (data_size * 96) / 100;
    size_t data_pos = 0;

    size_t train_pos = 0;
    size_t test_pos = 0;

    {
        boost::scoped_ptr<caffe::db::DB> train_db(caffe::db::GetDB("leveldb"));
        std::string dbTrainName("leela_train");
        train_db->Open(dbTrainName.c_str(), caffe::db::NEW);
        boost::scoped_ptr<caffe::db::Transaction> train_txn(train_db->NewTransaction());

        boost::scoped_ptr<caffe::db::DB> test_db(caffe::db::GetDB("leveldb"));
        std::string dbTestName("leela_test");
        test_db->Open(dbTestName.c_str(), caffe::db::NEW);
        boost::scoped_ptr<caffe::db::Transaction> test_txn(test_db->NewTransaction());

        for (auto it = data.begin(); it != data.end(); ++it) {
            TrainPosition& position = *it;
            int move = position.first;
            NNPlanes& nnplanes = position.second;
            caffe::Datum datum;
            datum.set_channels(22);
            datum.set_height(19);
            datum.set_width(19);
            datum.set_label((label_t)move);

            for (size_t p = 0; p < nnplanes.size(); p++) {
                for (size_t b = 0; b < nnplanes[p].size(); b++) {
                    datum.add_float_data((float)nnplanes[p][b]);
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
                if (test_pos % 1000 == 0) {
                    test_txn->Commit();
                    test_txn.reset(test_db->NewTransaction());
                }
            } else {
                std::stringstream ss;
                ss << train_pos;
                train_pos++;
                train_txn->Put(ss.str(), out);
                if (train_pos % 1000 == 0) {
                    train_txn->Commit();
                    train_txn.reset(train_db->NewTransaction());
                }
            }
        }
        train_txn->Commit();
        test_txn->Commit();
    }
    std::cout << train_pos << " training positions." << std::endl;
    std::cout << test_pos << " testing positions." << std::endl;
    return;

    data_pos = 0;
    for (auto it = data.begin(); it != data.end(); ++it) {
        TrainPosition& position = *it;
        int move = position.first;
        NNPlanes& nnplanes = position.second;
        vec_t vec;
        vec.resize(nnplanes.size() * nnplanes[0].size());
        int idx = 0;
        for (size_t p = 0; p < nnplanes.size(); p++) {
            for (size_t b = 0; b < nnplanes[p].size(); b++) {
                vec[idx++] = nnplanes[p][b];
            }
        }
        data_pos++;
        if (data_pos > traincut) {
            test_data.push_back(vec);
            test_label.push_back((label_t) move);
        } else {
            train_data.push_back(vec);
            train_label.push_back((label_t) move);
        }
    }
    data.clear();
    std::cout << train_data.size() << " training positions." << std::endl;
    std::cout << test_data.size() << " testing positions." << std::endl;

    progress_display disp(train_data.size());
    timer t;
    int minibatch_size = 128;
    nn.optimizer().alpha = 0.1;

    // test&save for each epoch
    int epoch = 0;

    auto on_enumerate_epoch = [&]() {
       std::cout << std::endl << t.elapsed() << "s elapsed." << std::endl;
       timer t2;
       result res = nn.test(test_data, test_label);
       std::cout << std::endl << t2.elapsed() << "s elapsed for test, "
                 << (test_data.size() / t2.elapsed()) << " pos/s." << std::endl;
       std::cout << "e=" << epoch << ", a=" << nn.optimizer().alpha << ", " << res.num_success
                 << "/" << res.num_total
                 << " ( " << res.num_success*100.0f/res.num_total << "%)"
                 << std::endl;
       std::ofstream ofs (("epoch_" + std::to_string(epoch++) + ".txt").c_str());
       ofs << nn;

       nn.optimizer().alpha *= 0.85; // decay learning rate
       nn.optimizer().alpha = std::max((tiny_cnn::float_t)0.00001, nn.optimizer().alpha);
       disp.restart(train_data.size());
       t.restart();
    };

    auto on_enumerate_minibatch = [&]() {
        disp += minibatch_size;
    };

    nn.train(train_data, train_label, minibatch_size, 50,
             on_enumerate_minibatch,
             on_enumerate_epoch);
}

void Network::autotune_from_file(std::string filename) {
    TrainVector data;

    gather_traindata(filename, data);

    train_network(data);
}
#endif
