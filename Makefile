default:
	$(MAKE) CC=gcc-7 CXX=g++-7 \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O3 -g -flax-vector-conversions -Wno-deprecated-declarations -ffast-math -march=native -fopenmp -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS) -fopenmp -g' \
		leela

gcc32b:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -pipe -O2 -g -m32 -std=c++11 -DNDEBUG -D_CONSOLE'  \
		LDFLAGS='$(LDFLAGS) -m32' \
		leela

debug:
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O0 -g -fopenmp -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -fopenmp -g' \
		leela

clang:
	$(MAKE) CC=cc CXX=c++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -O3 -ffast-math -g -march=native -std=c++11 -flto -D_CONSOLE -DNDEBUG' \
		LDFLAGS='$(LDFLAGS) -flto -fuse-linker-plugin' \
		leela

asan:
	$(MAKE) CC=clang-4.0 CXX=clang++-4.0 \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -O1 -g -fopenmp -std=c++11 -D_CONSOLE' \
		LDFLAGS='$(LDFLAGS) -g -fsanitize=address -fopenmp' \
		leela

LIBS = -lboost_program_options-mt
#DYNAMIC_LIBS += -lboost_system -lboost_filesystem -lcaffe-nv -lprotobuf -lglog
#LIBS += -lopenblas
DYNAMIC_LIBS += -lpthread
#DYNAMIC_LIBS += -lOpenCL
#LIBS += -framework Accelerate
LIBS += -framework OpenCL
#DYNAMIC_LIBS += -lmkl_rt

CAFFE_BASE = /usr/local
CAFFE_INC = $(CAFFE_BASE)/include
CAFFE_LIB = $(CAFFE_BASE)/lib
CXXFLAGS += -I$(CAFFE_INC) -I/usr/local/cuda/include
#CXXFLAGS += -I/opt/intel/mkl/include
#CXXFLAGS += -I/opt/OpenBLAS/include
#CXXFLAGS += -I/System/Library/Frameworks/Accelerate.framework/Versions/Current/Headers
LDFLAGS  += -L$(CAFFE_LIB)
#LDFLAGS  += -L/opt/intel/mkl/lib/intel64/
#LDFLAGS += -L/opt/OpenBLAS/lib

CXXFLAGS += -I.
CPPFLAGS += -MD -MP

sources = Network.cpp AttribScores.cpp FullBoard.cpp KoState.cpp Playout.cpp \
	  TimeControl.cpp UCTSearch.cpp Attributes.cpp Book.cpp \
	  GameState.cpp Leela.cpp PNNode.cpp SGFParser.cpp Timing.cpp \
	  Utils.cpp FastBoard.cpp Genetic.cpp Matcher.cpp PNSearch.cpp \
	  SGFTree.cpp TTable.cpp Zobrist.cpp FastState.cpp GTP.cpp \
	  MCOTable.cpp Random.cpp SMP.cpp UCTNode.cpp NN.cpp NNValue.cpp \
	  OpenCL.cpp MCPolicy.cpp

objects = $(sources:.cpp=.o)
deps = $(sources:%.cpp=%.d)

-include $(deps)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

leela: $(objects)
#	$(CXX) $(LDFLAGS) -o $@ $^ -static-libgcc -static-libstdc++ -Wl,-Bstatic $(LIBS) -Wl,-Bdynamic $(DYNAMIC_LIBS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS) $(DYNAMIC_LIBS)

clean:
	-$(RM) leela $(objects) $(deps)

.PHONY: clean default gcc32b debug llvm
