default:
	$(MAKE) CC=gcc CXX=g++ \
		CFLAGS='$(CFLAGS) -Wall -Wextra -pipe -flto -O3 -g -march=native -std=c++11 -DNDEBUG -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -flto -O3 -g -march=native -std=c++11 -DNDEBUG -D_CONSOLE'  \
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
		CFLAGS='$(CFLAGS) -Wall -Wextra -pipe -Og -g -std=c++11 -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -Og -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g' \
		leela

llvm:
	$(MAKE) CC=~/svn/llvm/build/bin/clang CXX=~/svn/llvm/build/bin/clang++ \
		CFLAGS='$(CFLAGS) -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -O1 -g -std=c++11 -D_CONSOLE' \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -O1 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g -fsanitize=address' \
		leela

LIBS = -lpthread -lboost_thread -lboost_system
LIBS += -ltbb -ltbbmalloc -lcaffe -lprotobuf -lglog

CAFFE_BASE = /usr/local
CAFFE_INC = $(CAFFE_BASE)/include
CAFFE_LIB = $(CAFFE_BASE)/lib
CFLAGS   += -I$(CAFFE_INC) -I/usr/local/cuda
CXXFLAGS += -I$(CAFFE_INC) -I/usr/local/cuda
LDFLAGS  += -L$(CAFFE_LIB)

CFLAGS   += -I.
CXXFLAGS += -I.

sources = Network.cpp AttribScores.cpp FullBoard.cpp KoState.cpp Playout.cpp \
	  Ruleset.cpp TimeControl.cpp UCTSearch.cpp Attributes.cpp \
	  GameState.cpp Leela.cpp PNNode.cpp SGFParser.cpp Timing.cpp \
	  Utils.cpp FastBoard.cpp Genetic.cpp Matcher.cpp PNSearch.cpp \
	  SGFTree.cpp TTable.cpp Zobrist.cpp FastState.cpp GTP.cpp \
	  MCOTable.cpp Random.cpp SMP.cpp UCTNode.cpp

objects = $(sources:.cpp=.o)

headers = $(wildcard *.h)

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(headers)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

leela:	$(objects)
	$(CXX) $(LDFLAGS) -o leela $(objects) $(LIBS)

clean:
	-$(RM) leela $(objects)

.PHONY: clean default gcc32b debug llvm
