#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include <string>
#include "GameState.h"

extern bool cfg_allow_pondering;
extern int cfg_num_threads;
extern int cfg_max_playouts;
extern bool cfg_enable_nets;
extern int cfg_lagbuffer_cs;
extern int cfg_mcnn_maturity;
extern int cfg_atari_give_expand;
extern int cfg_atari_escape_expand;
extern float cfg_cutoff_ratio;
extern float cfg_cutoff_offset;
extern float cfg_puct;
extern float cfg_perbias;
extern float cfg_easymove_ratio;
extern float cfg_crit_mine_1;
extern float cfg_crit_mine_2;
extern float cfg_crit_mine_3;
extern float cfg_crit_his_1;
extern float cfg_crit_his_2;
extern float cfg_crit_his_3;
extern float cfg_nearby;
extern float cfg_small_self_atari;
extern float cfg_medium_self_atari;
extern float cfg_big_self_atari;
extern float cfg_huge_self_atari;
extern float cfg_useless_self_atari;
extern float cfg_score_pow;
extern float cfg_cut;
extern float cfg_try_captures;
extern float cfg_try_critical;
extern float cfg_try_pattern;
extern float cfg_try_loops;

class GTP {
public:
    static bool execute(GameState &game, std::string xinput);
    static void setup_default_parameters();
private:
    static const int GTP_VERSION = 2;

    static std::string get_life_list(GameState & game, bool live);
    static const std::string s_commands[];
};


#endif
