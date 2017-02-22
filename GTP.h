#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include <string>
#include "GameState.h"

extern bool cfg_allow_pondering;
extern int cfg_num_threads;
extern int cfg_max_playouts;
extern bool cfg_enable_nets;
extern int cfg_mature_threshold;
extern float cfg_expand_divider;
extern int cfg_lagbuffer_cs;
#ifdef USE_OPENCL
extern int cfg_rowtiles;
#endif
extern float cfg_crit_mine_1;
extern float cfg_crit_mine_2;
extern float cfg_crit_his_1;
extern float cfg_crit_his_2;
extern float cfg_regular_self_atari;
extern float cfg_useless_self_atari;
extern float cfg_tactical;
extern float cfg_bound;
extern float cfg_pass_score;
extern float cfg_fpu;
extern float cfg_cutoff_offset;
extern float cfg_cutoff_ratio;
extern float cfg_puct;
extern float cfg_psa;
extern float cfg_beta;
extern float cfg_patternbonus;
extern float cfg_mix;
extern int cfg_eval_thresh;
extern int cfg_eval_scale;
extern int cfg_rave_min;
extern int cfg_rave_max;
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
