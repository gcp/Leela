#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#ifdef USE_OPTIONS
#include <boost/program_options.hpp>
#endif
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
#include "ThreadPool.h"
#include "MCPolicy.h"

using namespace Utils;

#ifdef USE_OPTIONS
void parse_commandline(int argc, char *argv[], bool & gtp_mode) {
    namespace po = boost::program_options;
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Show commandline options.")
        ("gtp,g", "Enable GTP mode.")
        ("threads,t", po::value<int>()->default_value(cfg_num_threads),
                      "Number of threads to use.")
        ("playouts,p", po::value<int>(),
                       "Limit number of playouts.")
        ("lagbuffer,b", po::value<int>()->default_value(cfg_lagbuffer_cs),
                        "Safety margin for time usage in centiseconds.")
        ("logfile,l", po::value<std::string>(), "File to log input/output to.")
        ("quiet,q", "Disable all diagnostic output.")
        ("komiadjust,k", "Adjust komi one point in my disadvantage (territory scoring).")
        ("noponder", "Disable pondering.")
        ("nonets", "Disable use of neural networks.")
        ("nobook", "Disable use of the fuseki library.")
#ifdef USE_OPENCL
        ("gpu",  po::value<std::vector<int> >(),
                "ID of the OpenCL device(s) to use (disables autodetection).")
        ("rowtiles", po::value<int>()->default_value(cfg_rowtiles),
                     "Split up the board in # tiles.")
#endif
#ifdef USE_TUNER
        ("mature_threshold", po::value<int>())
        ("expand_threshold", po::value<int>())
        ("beta", po::value<float>())
        ("patternbonus", po::value<float>())
        ("bound", po::value<float>())
        ("fpu", po::value<float>())
        ("puct", po::value<float>())
        ("uct", po::value<float>())
        ("psa", po::value<float>())
        ("cutoff_offset", po::value<float>())
        ("cutoff_ratio", po::value<float>())
        ("mix_opening", po::value<float>())
        ("mix_ending", po::value<float>())
        ("softmax_temp", po::value<float>())
        ("mc_softmax", po::value<float>())
        ("eval_thresh", po::value<int>())
        ("rave_moves", po::value<int>())
        ("extra_symmetry", po::value<int>())
        ("random_loops", po::value<int>())
#endif
        ;
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }  catch(boost::program_options::error& e) {
        myprintf("ERROR: %s\n", e.what());
        exit(EXIT_FAILURE);
    }

    // Handle commandline options
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (vm.count("quiet")) {
        cfg_quiet = true;
    }

#ifdef USE_TUNER
    if (vm.count("mature_threshold")) {
        cfg_mature_threshold = vm["mature_threshold"].as<int>();
    }
    if (vm.count("expand_threshold")) {
        cfg_expand_threshold = vm["expand_threshold"].as<int>();
    }
    if (vm.count("bound")) {
        cfg_bound = vm["bound"].as<float>();
    }
    if (vm.count("fpu")) {
        cfg_fpu = vm["fpu"].as<float>();
    }
    if (vm.count("puct")) {
        cfg_puct = vm["puct"].as<float>();
    }
    if (vm.count("uct")) {
        cfg_uct = vm["uct"].as<float>();
    }
    if (vm.count("psa")) {
        cfg_psa = vm["psa"].as<float>();
    }
    if (vm.count("softmax_temp")) {
        cfg_softmax_temp = vm["softmax_temp"].as<float>();
    }
    if (vm.count("mc_softmax")) {
        cfg_mc_softmax = vm["mc_softmax"].as<float>();
    }
    if (vm.count("beta")) {
        cfg_beta = vm["beta"].as<float>();
    }
    if (vm.count("patternbonus")) {
        cfg_patternbonus = vm["patternbonus"].as<float>();
    }
    if (vm.count("cutoff_offset")) {
        cfg_cutoff_offset = vm["cutoff_offset"].as<float>();
    }
    if (vm.count("cutoff_ratio")) {
        cfg_cutoff_ratio = vm["cutoff_ratio"].as<float>();
    }
    if (vm.count("mix_opening")) {
        cfg_mix_opening = vm["mix_opening"].as<float>();
    }
    if (vm.count("mix_ending")) {
        cfg_mix_ending = vm["mix_ending"].as<float>();
    }
    if (vm.count("eval_thresh")) {
        cfg_eval_thresh = vm["eval_thresh"].as<int>();
    }
    if (vm.count("rave_moves")) {
        cfg_rave_moves = vm["rave_moves"].as<int>();
    }
    if (vm.count("extra_symmetry")) {
        cfg_extra_symmetry = vm["extra_symmetry"].as<int>();
    }
    if (vm.count("random_loops")) {
        cfg_random_loops = vm["random_loops"].as<int>();
    }
#endif

    if (vm.count("logfile")) {
        cfg_logfile = vm["logfile"].as<std::string>();
        myprintf("Logging to %s.\n", cfg_logfile.c_str());
        cfg_logfile_handle = fopen(cfg_logfile.c_str(), "a");
    }

    if (vm.count("gtp")) {
        gtp_mode = true;
    }

    if (vm.count("threads")) {
        int num_threads = vm["threads"].as<int>();
        if (num_threads > cfg_num_threads) {
            myprintf("Clamping threads to maximum = %d\n", cfg_num_threads);
        } else if (num_threads != cfg_num_threads) {
            myprintf("Using %d thread(s).\n", num_threads);
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

    if (vm.count("nobook")) {
        cfg_allow_book = false;
    }

    if (vm.count("komiadjust")) {
        myprintf("Adjusting komi for territory scoring rules.\n");
        cfg_komi_adjust = true;
    }

    if (vm.count("lagbuffer")) {
        int lagbuffer = vm["lagbuffer"].as<int>();
        if (lagbuffer != cfg_lagbuffer_cs) {
            myprintf("Using per-move time margin of %.2fs.\n", lagbuffer/100.0f);
            cfg_lagbuffer_cs = lagbuffer;
        }
    }

#ifdef USE_OPENCL
    if (vm.count("gpu")) {
        cfg_gpus = vm["gpu"].as<std::vector<int> >();
    }

    if (vm.count("rowtiles")) {
        int rowtiles = vm["rowtiles"].as<int>();
        rowtiles = std::min(19, rowtiles);
        rowtiles = std::max(1, rowtiles);
        if (rowtiles != cfg_rowtiles) {
            myprintf("Splitting the board in %d tiles.\n", rowtiles);
            cfg_rowtiles = rowtiles;
        }
    }
#endif
}
#endif

#ifdef _CONSOLE
int main (int argc, char *argv[]) {
    bool gtp_mode = false;
    std::string input;

#ifdef USE_CAFFE
    ::google::InitGoogleLogging(argv[0]);
#endif
    // Set up engine parameters
    GTP::setup_default_parameters();
#ifdef USE_OPTIONS
    parse_commandline(argc, argv, gtp_mode);
#endif

    // Disable IO buffering as much as possible
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    std::cin.setf(std::ios::unitbuf);

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#ifndef WIN32
    setbuf(stdin, NULL);
#endif

    thread_pool.initialize(cfg_num_threads);

    // Use deterministic random numbers for hashing
    std::unique_ptr<Random> rng(new Random(5489));
    Zobrist::init_zobrist(*rng);

    // Initialize things
    AttribScores::get_attribscores();
    Matcher::get_Matcher();
    Network::get_Network();

    std::unique_ptr<GameState> maingame(new GameState);

    /* set board limits */
    float komi = 7.5;
    maingame->init_game(19, komi);

    if (!GTP::perform_self_test(*maingame)) {
        exit(EXIT_FAILURE);
    }

    for(;;) {
        if (!gtp_mode) {
            maingame->display_state();
            std::cout << "Leela: ";
        }

        if (std::getline(std::cin, input)) {
            Utils::log_input(input);
            GTP::execute(*maingame, input);
        } else {
            // eof or other error
            break;
        }

        // Force a flush of the logfile
        if (cfg_logfile_handle) {
            fclose(cfg_logfile_handle);
            cfg_logfile_handle = fopen(cfg_logfile.c_str(), "a");
        }
    }

    return 0;
}
#endif
