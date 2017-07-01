#include "config.h"

#include <memory>
#include <cmath>

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
        // int handicap = sgftree->get_state()->get_handicap();

        int movecount = sgftree->count_mainline_moves();
        int move_pick = Random::get_Rng()->randint32(movecount);
        // GameState state = sgftree->follow_mainline_state(move_pick);
        KoState * state = sgftree->get_state_from_mainline(move_pick);

        if (who_won != FastBoard::BLACK && who_won != FastBoard::WHITE) {
            continue;
        }
        bool blackwon = (who_won == FastBoard::BLACK);

        int iterations = 512;
        float mcscore = Playout::mc_owner(*state, iterations);
        float nwscore = Network::get_Network()->get_value(
            state, Network::Ensemble::AVERAGE_ALL);

        if (state->get_to_move() == FastBoard::WHITE) {
            mcscore = 1.0f - mcscore;
            nwscore = 1.0f - nwscore;
        }

        myprintf("n=%d BW: %d Score: %1.3f NN: %1.3f ",
                 count, blackwon, mcscore, nwscore);

        sum_sq_pp += std::pow((blackwon ? 1.0f : 0.0f) - mcscore, 2.0f);
        sum_sq_nn += std::pow((blackwon ? 1.0f : 0.0f) - nwscore, 2.0f);
        count++;

        myprintf(" MSE MC=%1.3f MSE NN=%1.3f\n",
                 sum_sq_pp/(double)count,
                 sum_sq_nn/(double)count);
    }
}