#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include <string>
#include <vector>
#include "GameState.h"

extern bool cfg_allow_pondering;
extern bool cfg_allow_book;
extern int cfg_num_threads;
extern int cfg_max_playouts;
extern bool cfg_enable_nets;
extern bool cfg_komi_adjust;
extern int cfg_lagbuffer_cs;
#ifdef USE_OPENCL
extern std::vector<int> cfg_gpus;
extern int cfg_rowtiles;
#endif
extern float cfg_bound;
extern float cfg_fpu;
extern float cfg_cutoff_offset;
extern float cfg_cutoff_ratio;
extern float cfg_puct;
extern float cfg_psa;
extern float cfg_softmax_temp;
extern float cfg_mc_softmax;
extern int cfg_extra_symmetry;
extern int cfg_random_loops;
extern std::string cfg_logfile;
extern FILE* cfg_logfile_handle;
extern bool cfg_quiet;

class GTP {
public:
    static bool execute(GameState & game, std::string xinput);
    static void setup_default_parameters();
    static bool perform_self_test(GameState & state);
private:
    static const int GTP_VERSION = 2;

    static std::string get_life_list(GameState & game, bool live);
    static const std::string s_commands[];
};


#endif
