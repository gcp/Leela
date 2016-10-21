#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#ifdef USE_CAFFE
#include <glog/logging.h>
#endif
#include "Network.h"

#include "Zobrist.h"
#include "GTP.h"
#include "SMP.h"
#include "Random.h"
#include "Utils.h"
#include "Matcher.h"
#include "AttribScores.h"

using namespace Utils;

#ifdef _CONSOLE
int main (int argc, char *argv[]) {
    int done = false;
    int gtp_mode;
    std::string input;

#ifdef USE_CAFFE
    ::google::InitGoogleLogging(argv[0]);
#endif
    // Defaults
    gtp_mode = false;
    GTP::setup_default_parameters();

    namespace po = boost::program_options;
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Show commandline options.")
        ("gtp,g", "Enable GTP mode.")
        ("threads,t", po::value<int>()->default_value(cfg_num_threads),
                      "Number of threads to use.")
        ("playouts,p", po::value<int>(), "Limit number of playouts.")
        ("lagbuffer,b", po::value<int>()->default_value(200),
                      "Safety margin for time usage in centiseconds.")
        ("noponder", "Disable pondering.")
        ("nonets", "Disable use of neural networks.")
#ifdef USE_TUNER
        ("maturity", po::value<int>())
        ("atari_give", po::value<int>())
        ("atari_escape", po::value<int>())
        ("cutoff_ratio", po::value<float>())
        ("cutoff_offset", po::value<float>())
        ("puct", po::value<float>())
        ("perbias", po::value<float>())
        ("easymove", po::value<float>())
        ("crit_mine_1", po::value<float>())
        ("crit_mine_2", po::value<float>())
        ("crit_mine_3", po::value<float>())
        ("crit_his_1", po::value<float>())
        ("crit_his_2", po::value<float>())
        ("crit_his_3", po::value<float>())
        ("tactical", po::value<float>())
        ("small_self_atari", po::value<float>())
        ("big_self_atari", po::value<float>())
        ("bad_self_atari", po::value<float>())
        ("useless_self_atari", po::value<float>())
        ("score_pow", po::value<float>())
        ("try_captures", po::value<float>())
        ("try_critical", po::value<float>())
        ("try_pattern", po::value<float>())
        ("try_loops", po::value<float>())
#endif
        ;
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }  catch(boost::program_options::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        return 1;
    }

    // Handle commandline options
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

#ifdef USE_TUNER
    if (vm.count("maturity")) {
        cfg_mcnn_maturity = vm["maturity"].as<int>();
    }
    if (vm.count("atari_give")) {
        cfg_atari_give_expand = vm["atari_give"].as<int>();
    }
    if (vm.count("atari_escape")) {
        cfg_atari_escape_expand = vm["atari_escape"].as<int>();
    }
    if (vm.count("cutoff_ratio")) {
        cfg_cutoff_ratio = vm["cutoff_ratio"].as<float>();
    }
    if (vm.count("cutoff_offset")) {
        cfg_cutoff_offset = vm["cutoff_offset"].as<float>();
    }
    if (vm.count("puct")) {
        cfg_puct = vm["puct"].as<float>();
    }
    if (vm.count("perbias")) {
        cfg_perbias = vm["perbias"].as<float>();
    }
    if (vm.count("easymove")) {
        cfg_easymove_ratio = vm["easymove"].as<float>();
    }
    if (vm.count("crit_mine_1")) {
        cfg_crit_mine_1 = vm["crit_mine_1"].as<float>();
    }
    if (vm.count("crit_mine_2")) {
        cfg_crit_mine_2 = vm["crit_mine_2"].as<float>();
    }
    if (vm.count("crit_mine_3")) {
        cfg_crit_mine_3 = vm["crit_mine_3"].as<float>();
    }
    if (vm.count("crit_his_1")) {
        cfg_crit_his_1 = vm["crit_his_1"].as<float>();
    }
    if (vm.count("crit_his_2")) {
        cfg_crit_his_2 = vm["crit_his_2"].as<float>();
    }
    if (vm.count("crit_his_3")) {
        cfg_crit_his_3 = vm["crit_his_3"].as<float>();
    }
    if (vm.count("tactical")) {
        cfg_tactical = vm["tactical"].as<float>();
    }
    if (vm.count("small_self_atari")) {
        cfg_small_self_atari = vm["small_self_atari"].as<float>();
    }
    if (vm.count("big_self_atari")) {
        cfg_big_self_atari = vm["big_self_atari"].as<float>();
    }
    if (vm.count("bad_self_atari")) {
        cfg_bad_self_atari = vm["bad_self_atari"].as<float>();
    }
    if (vm.count("useless_self_atari")) {
        cfg_useless_self_atari = vm["useless_self_atari"].as<float>();
    }
    if (vm.count("score_pow")) {
        cfg_score_pow = vm["score_pow"].as<float>();
    }
    if (vm.count("try_captures")) {
        cfg_try_captures = vm["try_captures"].as<float>();
    }
    if (vm.count("try_critical")) {
        cfg_try_critical = vm["try_critical"].as<float>();
    }
    if (vm.count("try_pattern")) {
        cfg_try_pattern = vm["try_pattern"].as<float>();
    }
#endif

    if (vm.count("gtp")) {
        gtp_mode = true;
    }

    if (vm.count("threads")) {
        int num_threads = vm["threads"].as<int>();
        if (num_threads > cfg_num_threads) {
            std::cerr << "Clamping threads to maximum = " << cfg_num_threads
                      << std::endl;
        } else if (num_threads != cfg_num_threads) {
            std::cerr << "Using " << num_threads << " thread(s)." << std::endl;
            cfg_num_threads = num_threads;
        }
    }

    if (vm.count("noponder")) {
        cfg_allow_pondering = false;
    }

    if (vm.count("playouts")) {
        cfg_max_playouts = vm["playouts"].as<int>();
    }

    if (vm.count("nonets")) {
        cfg_enable_nets = false;
    }

    if (vm.count("lagbuffer")) {
        int lagbuffer = vm["lagbuffer"].as<int>();
        if (lagbuffer != cfg_lagbuffer_cs) {
            std::cerr << boost::format("Using per-move time margin of %.2fs.\n")
                % (lagbuffer/100.0f);
            cfg_lagbuffer_cs = lagbuffer;
        }
    }

    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    std::cin.setf(std::ios::unitbuf);

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#ifndef WIN32
    setbuf(stdin, NULL);
#endif

    // Use deterministic random numbers for hashing
    std::unique_ptr<Random> rng(new Random(5489UL));
    Zobrist::init_zobrist(*rng);

    // Now seed with something more random
    rng.reset(new Random());
    AttribScores::get_attribscores();
    Matcher::get_Matcher();
    Network::get_Network();

    std::unique_ptr<GameState> maingame(new GameState);

    /* set board limits */
    float komi = 7.5;
    maingame->init_game(19, komi);

    while (!done) {
        if (!gtp_mode) {
            maingame->display_state();
            std::cout << "Leela: ";
        }

        std::getline(std::cin, input);
        GTP::execute(*maingame, input);
    }
    return 0;
}
#endif
