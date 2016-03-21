default:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -pipe -O3 -g -march=native -std=c++11 -DNDEBUG -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O3 -g -march=native -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS)' \
		leela

gcc32b:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -pipe -O2  -g -m32 -std=c++11 -DNDEBUG -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O2 -g -m32 -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS) -m32' \
		leela

debug:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -pipe -O0 -g -std=c++11 -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O0 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g' \
		leela

llvm:
	$(MAKE) CC=~/svn/llvm/build/bin/clang CXX=~/svn/llvm/build/bin/clang++ \
		CFLAGS='$(CFLAGS) -Wall -fsanitize=address -fno-omit-frame-pointer -O0 -g -std=c++11 -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -fsanitize=address -fno-omit-frame-pointer -O0 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g -fsanitize=address' \
		leela

LIBS = -lpthread -lboost_thread -lboost_system -ltbb -ltbbmalloc -lcaffe -lprotobuf -lglog

CAFFE_BASE = /usr/local
CAFFE_INC = $(CAFFE_BASE)/include
CAFFE_LIB = $(CAFFE_BASE)/lib
LDFLAGS += -L$(CAFFE_LIB)

sources = Network.cpp AttribScores.cpp FullBoard.cpp KoState.cpp Playout.cpp \
	  Ruleset.cpp TimeControl.cpp UCTSearch.cpp Attributes.cpp \
	  GameState.cpp Leela.cpp PNNode.cpp SGFParser.cpp Timing.cpp \
	  Utils.cpp FastBoard.cpp Genetic.cpp Matcher.cpp PNSearch.cpp \
	  SGFTree.cpp TTable.cpp Zobrist.cpp FastState.cpp GTP.cpp \
	  MCOTable.cpp Random.cpp SMP.cpp UCTNode.cpp

objects = $(sources:.cpp=.o)

headers = $(wildcard *.h)

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -I. -I$(CAFFE_INC) -c -o $@ $<

%.o: %.cpp $(headers)
	$(CXX) $(CXXFLAGS) -I. -I$(CAFFE_INC) -c -o $@ $<

leela:	$(objects)
	$(CXX) $(LDFLAGS) -o leela $(objects) $(LIBS)

clean:
	-$(RM) leela $(objects)

.PHONY: clean default gcc32b debug llvm
