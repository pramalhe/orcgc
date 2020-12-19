CXX = g++-9
CXXFLAGS = -std=c++17 -g -O2 -DALWAYS_USE_EXCHANGE #-fsanitize=address # -O2 # 

INCLUDES = -I../ -I../common/ 


BINARIES = \
	bin/q-ll-enq-deq \
	bin/set-ll-1k \
	bin/set-skiplist-1m \
	bin/set-tree-1m \

CSRCS = \
	../common/ThreadRegistry.cpp \
	
TRACKERS_DEP = \
	../trackers/OrcPTP.hpp \
	../trackers/HazardPointers.hpp \
	../trackers/PassTheBuck.hpp \
	../trackers/PassThePointer.hpp \

SRC_TREES = \
	../datastructures/trees/NatarajanTreeOrcGC.hpp \

QUEUES_DEP = \
	../datastructures/queues/LCRQueue.hpp \
	../datastructures/queues/MichaelScottQueue.hpp \
	../datastructures/queues/MichaelScottQueueOrcGC.hpp \
	../datastructures/queues/TurnQueue.hpp \

STACKS_DEP = \
	../datastructures/stacks/TreiberStack.hpp \
	../datastructures/stacks/TreiberStackOrcGC.hpp \
	
SKIPLIST_DEP = \
	../datastructures/skiplists/HerlihyShavitLockFreeSkipListOrcGC.hpp \
	../datastructures/skiplists/HerlihyShavitLockFreeSkipListOrcGCOrig.hpp \


all: $(BINARIES)


clean:
	rm -f bin/q-*
	rm -f bin/stack-*
	rm -f bin/set-*






#
# Queues for volatile memory
#	
bin/q-ll-enq-deq: q-ll-enq-deq.cpp $(QUEUES_DEP) $(TRACKERS_DEP) BenchmarkQueues.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) q-ll-enq-deq.cpp -o bin/q-ll-enq-deq -lpthread
	

#
# Stacks
#
bin/stack-ll: stack-ll.cpp $(STACKS_DEP) $(TRACKERS_DEP) BenchmarkStacks.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) stack-ll.cpp -o bin/stack-ll -lpthread
	


	
#
# Sets for volatile memory
#	
bin/set-ll-1k: set-ll-1k.cpp $(STMS) $(SRC_LISTS) $(TRACKERS_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) set-ll-1k.cpp -o bin/set-ll-1k -lpthread

bin/set-skiplist-1m: set-skiplist-1m.cpp $(STMS) $(SKIPLIST_DEP) $(TRACKERS_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) set-skiplist-1m.cpp -o bin/set-skiplist-1m -lpthread $(ESTM_LIB)

bin/set-tree-1m: set-tree-1m.cpp $(STMS) $(SRC_TREES) $(TRACKERS_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) set-tree-1m.cpp -o bin/set-tree-1m -lpthread



