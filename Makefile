default:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -pipe -O3 -g -std=c++11 -DNDEBUG -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O3 -g -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS) -lpthread -lboost_thread -lboost_system' \
		leela

gcc32b:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -pipe -O2  -g -m32 -std=c++11 -DNDEBUG -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O2 -g -m32 -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS) -m32 -lpthread -lboost_thread -lboost_system' \
		leela

debug:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -pipe -O0 -g -std=c++11 -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O0 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g -lpthread -lboost_thread -lboost_system' \
		leela

llvm:
	$(MAKE) CC=~/svn/llvm/build/bin/clang CXX=~/svn/llvm/build/bin/clang++ \
		CFLAGS='$(CFLAGS) -Wall -fsanitize=address -fno-omit-frame-pointer -O0 -g -std=c++11 -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -fsanitize=address -fno-omit-frame-pointer -O0 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g -fsanitize=address -lpthread -lboost_thread -lboost_system' \
		leela

sources = AttribScores.cpp FullBoard.cpp KoState.cpp Playout.cpp \
	  Ruleset.cpp TimeControl.cpp UCTSearch.cpp Attributes.cpp \
	  GameState.cpp Leela.cpp PNNode.cpp SGFParser.cpp Timing.cpp \
	  Utils.cpp FastBoard.cpp Genetic.cpp Matcher.cpp PNSearch.cpp \
	  SGFTree.cpp TTable.cpp Zobrist.cpp FastState.cpp GTP.cpp \
	  MCOTable.cpp Random.cpp SMP.cpp UCTNode.cpp

objects = $(sources:.cpp=.o)

headers = $(wildcard *.h)

%.o: %.c $(headers)
	$(CC) $(CFLAGS)    -c -o $@ $<

%.o: %.cpp $(headers)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

leela:	$(objects)
	$(CXX) $(LDFLAGS) $(LIBS) -o leela $(objects)

clean:
	-$(RM) leela $(objects)

.PHONY: clean default gcc32b debug llvm
