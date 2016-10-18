default:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O3 -g -ffast-math -march=native -flto -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS)' \
		leela

gcc32b:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O2 -g -m32 -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS) -m32' \
		leela

debug:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O0 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g' \
		leela

llvm:
	$(MAKE) CC=~/svn/llvm/build/bin/clang CXX=~/svn/llvm/build/bin/clang++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -O3 -ffast-math -g -march=native -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS)' \
		leela

asan:
	$(MAKE) CC=~/svn/llvm/build/bin/clang CXX=~/svn/llvm/build/bin/clang++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -O1 -g -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g -fsanitize=address' \
		leela

LIBS = -lboost_thread -lboost_system -lboost_program_options
#LIBS += -lboost_filesystem -lcaffe -lprotobuf -lglog
#LIBS += -lopenblas
DYNAMIC_LIBS += -lOpenCL
#LIBS += -framework Accelerate
#LIBS += -framework OpenCL
#LIBS += -lmkl_rt

CAFFE_BASE = /usr/local
CAFFE_INC = $(CAFFE_BASE)/include
CAFFE_LIB = $(CAFFE_BASE)/lib
CXXFLAGS += -I$(CAFFE_INC) -I/usr/local/cuda/include
#CXXFLAGS += -I/System/Library/Frameworks/Accelerate.framework/Versions/Current/Headers
LDFLAGS  += -L$(CAFFE_LIB)
#LDFLAGS  += -L/opt/intel/mkl/lib/intel64/

CXXFLAGS += -I.
CPPFLAGS += -MD -MP

sources = Network.cpp AttribScores.cpp FullBoard.cpp KoState.cpp Playout.cpp \
	  TimeControl.cpp UCTSearch.cpp Attributes.cpp Book.cpp \
	  GameState.cpp Leela.cpp PNNode.cpp SGFParser.cpp Timing.cpp \
	  Utils.cpp FastBoard.cpp Genetic.cpp Matcher.cpp PNSearch.cpp \
	  SGFTree.cpp TTable.cpp Zobrist.cpp FastState.cpp GTP.cpp \
	  MCOTable.cpp Random.cpp SMP.cpp UCTNode.cpp NN.cpp OpenCL.cpp

objects = $(sources:.cpp=.o)
deps = $(sources:%.cpp=%.d)

-include $(deps)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

leela: $(objects)
#	$(CXX) $(LDFLAGS) -o $@ $^ -static-libgcc -static-libstdc++ -Wl,-Bstatic $(LIBS) -Wl,-Bdynamic -lpthread $(DYNAMIC_LIBS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS) $(DYNAMIC_LIBS)

clean:
	-$(RM) leela $(objects) $(deps)

.PHONY: clean default gcc32b debug llvm
