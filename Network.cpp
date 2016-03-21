#include <algorithm>
#include <cassert>
#include <list>
#include <set>
#include <fstream>
#include <memory>
#include <boost/tr1/array.hpp>
#include <boost/scoped_ptr.hpp>
#include <caffe/proto/caffe.pb.h>
#include <caffe/util/db.hpp>
#include <caffe/util/io.hpp>

#include "tiny_cnn/tiny_cnn.h"

#include "config.h"

#include "SGFTree.h"
#include "SGFParser.h"
#include "Utils.h"
#include "FastBoard.h"
#include "Random.h"
#include "Network.h"

using namespace Utils;

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

            int skip = Random::get_Rng()->randint(3);
            // check every 3rd move
            if (skip == 0) {
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

                // sitting at a state, with the move actually played in move
                // gather feature sets of all moves
                std::vector<int> moves = state->generate_moves(tomove);
                bool moveseen = false;

                BoardPlane empt_color;   // 0
                BoardPlane move_color;   // 1
                BoardPlane othr_color;   // 2
                BoardPlane lastmoves;    // 3
                BoardPlane libs_1;       // 4
                BoardPlane libs_2;       // 5
                BoardPlane libs_3;       // 6
                BoardPlane libs_4p;      // 7
                BoardPlane libs_5p;      // 7
                BoardPlane libs_1_e;     // 8
                BoardPlane libs_2_e;     // 9
                BoardPlane libs_3_e;     //10
                BoardPlane libs_4p_e;    //11
                BoardPlane libs_5p_e;    //11
                BoardPlane after_1;      //10
                BoardPlane after_2;      //11
                BoardPlane after_3;      //12
                BoardPlane after_4p;     //13
                BoardPlane after_1_e;    //14
                BoardPlane after_2_e;    //15
                BoardPlane after_3_e;    //16
                BoardPlane after_4p_e;   //17
                BoardPlane ssize_1;      //18
                BoardPlane ssize_2;      //19
                BoardPlane ssize_3;      //20
                BoardPlane ssize_4;     //21
                BoardPlane ssize_5;      //21
                BoardPlane ssize_6p;
                BoardPlane komove;
                BoardPlane ladder;       //22
                BoardPlane string_h1;    //21
                BoardPlane string_h2;    //22
                BoardPlane string_h3;    //23
                BoardPlane string_h4;    //24
                // randomly spread strings through planes
                int hidx = Random::get_Rng()->randint(4);

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
                            } else if (rlibs == 4) {
                                if (color == tomove) {
                                    libs_4p[idx] = true;
                                    move_color[idx] = true;
                                } else {
                                    libs_4p_e[idx] = true;
                                    othr_color[idx] = true;
                                }
                                assert(rlibs >= 4);
                            }
                            else if (rlibs >= 5) {
                                if (color == tomove) {
                                    libs_5p[idx] = true;
                                    move_color[idx] = true;
                                } else {
                                    libs_5p_e[idx] = true;
                                    othr_color[idx] = true;
                                }
                            }
                            int ss = state->board.string_size(vtx);
                            if (ss == 1) {
                                ssize_1[idx] = true;
                            } else if (ss == 2) {
                                ssize_2[idx] = true;
                            } else if (ss == 3) {
                                ssize_3[idx] = true;
                            } else if (ss == 4) {
                                ssize_4[idx] = true;
                            } else if (ss == 5) {
                                ssize_5[idx] = true;
                            } else if (ss >= 6) {
                                ssize_6p[idx] = true;
                            }
                            int pid = state->board.get_groupid(vtx);
                            int gidx = (pid + hidx) % 4;
                            if (gidx == 0) {
                                string_h1[idx] = true;
                            } else if (gidx == 1) {
                                string_h2[idx] = true;
                            } else if (gidx == 2) {
                                string_h3[idx] = true;
                            } else if (gidx == 3) {
                                string_h4[idx] = true;
                            }
                        } else {
                            empt_color[idx] = true;

                            //int al = state->board.minimum_elib_count(!tomove, vtx);
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
                            //int at = state->board.minimum_elib_count(tomove, vtx);
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
                    std::pair<int, int> kopair = state->board.get_xy(state->get_komove());
                    int idx = kopair.second * 19 + kopair.first;
                    komove[idx] = true;
                }

                TrainPosition position;

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
                    position.second.resize(22);
                    position.second[0] = empt_color;
                    position.second[1] = move_color;
                    position.second[2] = othr_color;
                    position.second[3] = libs_1;
                    position.second[4] = libs_2;
                    position.second[5] = libs_3;
                    position.second[6] = libs_4p;
                    position.second[7] = libs_5p;
                    position.second[8] = libs_1_e;
                    position.second[9] = libs_2_e;
                    position.second[10] = libs_3_e;
                    position.second[11] = libs_4p_e;
                    position.second[12] = libs_5p_e;
                    position.second[13] = lastmoves;
                    //position.second[14] = after_1;
                    //position.second[15] = after_2;
                    //position.second[16] = after_3;
                    //position.second[17] = after_4p;
                    //position.second[18] = after_1_e;
                    //position.second[19] = after_2_e;
                    //position.second[20] = after_3_e;
                    //position.second[21] = after_4p_e;
                    position.second[14] = ssize_1;
                    position.second[15] = ssize_2;
                    position.second[16] = ssize_3;
                    position.second[17] = ssize_4;
                    position.second[18] = ssize_5;
                    position.second[19] = ssize_6p;
                    position.second[20] = ladder;
                    position.second[21] = komove;
                    //position.second[30] = string_h1;
                    //position.second[31] = string_h2;
                    //position.second[32] = string_h3;
                    //position.second[33] = string_h4;
                    /*     position.second.resize(10);
                    position.second[0] = empt_color;
                    position.second[1] = libs_1;
                    position.second[2] = libs_2;
                    position.second[3] = libs_3;
                    position.second[4] = libs_4p;
                    position.second[5] = libs_1_e;
                    position.second[6] = libs_2_e;
                    position.second[7] = libs_3_e;
                    position.second[8] = libs_4p_e;
                    position.second[9] = lastmoves;*/
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
