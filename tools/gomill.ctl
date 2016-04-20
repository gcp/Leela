competition_type = 'playoff'

record_games = False
stderr_to_log = True

players = {
    'gnugo-l10' : Player("gnugo --mode=gtp --chinese-rules "
                        "--level=10 "
                        "--positional-superko --capture-all-dead "
                        "--score aftermath --play-out-aftermath",
                  ),
    'leela-dcnn2' : Player("./leela-050dcnn2 gtp ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
			   environ={
				'OPENBLAS_NUM_THREADS':'1',
				'OPENBLAS_CORETYPE':'Steamroller',
			   },
                    ),
    'leela-mcts050' : Player("./leela-050mcts gtp ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
                    ),
}

board_size = 19
komi = 7.5

matchups = [
	Matchup('leela-mcts050', 'leela-dcnn2',
                scorer='players', number_of_games=1000),
]
