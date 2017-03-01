#include "config.h"

#include <memory>
#include <cmath>
#include <omp.h>

#include "MCPolicy.h"
#include "SGFParser.h"
#include "SGFTree.h"
#include "Utils.h"
#include "Random.h"
#include "Network.h"
#include "Playout.h"

using namespace Utils;

void MCPolicy::mse_from_file(std::string filename) {
    std::vector<std::string> games = SGFParser::chop_all(filename);
    size_t gametotal = games.size();
    myprintf("Total games in file: %d\n", gametotal);

    double sum_sq_pp = 0.0;
    double sum_sq_nn = 0.0;
    int count = 0;

    while (1) {
        int pick = Random::get_Rng()->randint32(gametotal);

        std::unique_ptr<SGFTree> sgftree(new SGFTree);
        try {
            sgftree->load_from_string(games[pick]);
        } catch (...) {
        };

        int who_won = sgftree->get_winner();
        int handicap = sgftree->get_state()->get_handicap();

        int movecount = sgftree->count_mainline_moves();
        int move_pick = Random::get_Rng()->randint32(movecount);
        // GameState state = sgftree->follow_mainline_state(move_pick);
        KoState * state = sgftree->get_state_from_mainline(move_pick);

        if (who_won != FastBoard::BLACK && who_won != FastBoard::WHITE) {
            continue;
        }
        bool blackwon = (who_won == FastBoard::BLACK);

        float bwins = 0.0f;
        constexpr int iterations = 128;
        #pragma omp parallel for reduction (+:bwins)
        for (int i = 0; i < iterations; i++) {
            FastState tmp = *state;

            Playout p;
            p.run(tmp, false, true);

            float score = p.get_score();
            if (score > 0.0f) {
                bwins += 1.0f;
            }
        }
        bwins /= (float)iterations;

        float nwscore = Network::get_Network()->get_value(
            state, Network::Ensemble::RANDOM_ROTATION);

        if (state->get_to_move() == FastBoard::WHITE) {
            nwscore = 1.0f - nwscore;
        }

        myprintf("n=%d BW: %d Score: %1.4f NN: %1.4f ",
                 count, blackwon, bwins, nwscore);

        sum_sq_pp += std::pow(2.0f*((blackwon ? 1.0f : 0.0f) - bwins),   2.0f);
        sum_sq_nn += std::pow(2.0f*((blackwon ? 1.0f : 0.0f) - nwscore), 2.0f);
        count++;

        myprintf(" MSE MC=%1.4f MSE NN=%1.4f\n",
                 sum_sq_pp/((double)2.0*count),
                 sum_sq_nn/((double)2.0*count));
    }
}