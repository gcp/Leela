competition_type = 'playoff'

record_games = True
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
				'OPENBLAS_CORETYPE':'Haswell',
			   },
                    ),
    'leela-mcts050' : Player("./leela-050mcts gtp ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
                    ),
    'leela-xxx' : Player("./leela-xxx --gtp --threads 1 --noponder ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
                    ),
    'leela-073e' : Player("./leela-073e --gtp --threads 1 --noponder ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
                    ),
    'leela-080' : Player("./leela-080 --gtp --threads 1 --noponder ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
                    ),
    'leela-080va' : Player("./leela-080va --gtp --threads 1 --noponder ",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
                    ),
    'pachi' : Player("./pachi -t _600 threads=1,pondering=0,max_tree_size=3072 -f book2.dat", cwd="~/git/pachi"),
    'Leela_0.6.2' : Player("./leela_062_linux_x64 --threads 2 --noponder --gtp",
                           startup_gtp_commands=[
				"time_settings 600 0 0",
                           ],
     )
}

alternating = True
board_size = 19
komi = 7.5

matchups = [
	Matchup('leela-080', 'leela-080va',
                scorer='players', number_of_games=1000),
#	Matchup('leela-073e', 'leela-075j',
#                scorer='players', number_of_games=1000),
#	Matchup('leela-075i', 'leela-075j',
#               scorer='players', number_of_games=1000),
#	Matchup('pachi', 'leela-073e', alternating=False, handicap=8,
#               scorer='players', number_of_games=1000),
#	Matchup('pachi', 'leela-075i', alternating=False, handicap=8,
#               scorer='players', number_of_games=1000),
#	Matchup('pachi', 'leela-075j', alternating=False, handicap=8,
#                scorer='players', number_of_games=1000),
]
