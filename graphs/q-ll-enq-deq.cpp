/*
 * Executes the following non-blocking (linked list based) queues in a single-enqueue-single-dequeue benchmark:
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "BenchmarkQueues.hpp"
#include "common/CmdLineConfig.hpp"
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"
#include "trackers/PassTheBuck.hpp"
#include "datastructures/queues/KoganPetrankQueueOrcGC.hpp"
#include "datastructures/queues/LCRQueue.hpp"
#include "datastructures/queues/LCRQueueOrcGC.hpp"
#include "datastructures/queues/MichaelScottQueue.hpp"
#include "datastructures/queues/MichaelScottQueueOrcGC.hpp"
#include "datastructures/queues/TurnQueue.hpp"
#include "datastructures/queues/TurnQueueOrcGC.hpp"
//#include "datastructures/queues/SimQueue.hpp"


#define MILLION  1000000LL

int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    const std::string dataFilename { "data/q-ll.txt" };
    const long numPairs = 10*MILLION;                                  // 10M is fast enough on the laptop, but on AWS we can use 100M
    const int EMAX_CLASS = 100;
    uint64_t results[EMAX_CLASS][cfg.threads.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*cfg.threads.size());

    // Enq-Deq Throughput benchmarks
    for (int it = 0; it < cfg.threads.size(); it++) {
        int nThreads = cfg.threads[it];
        int ic = 0;
        BenchmarkQueues bench(nThreads);
        std::cout << "\n----- q-ll-enq-deq   threads=" << nThreads << "   pairs=" << numPairs/MILLION << "M   runs=" << cfg.runs << " -----\n";

        // Maged Michael and Michael Scott's lock-free queue
        results[ic][it] = bench.enqDeq<MichaelScottQueue<UserData,HazardPointers>>    (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<MichaelScottQueue<UserData,PassTheBuck>>       (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<MichaelScottQueue<UserData,PassThePointer>>    (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<MichaelScottQueueOrcGC<UserData>>              (cNames[ic], numPairs, cfg.runs);
        ic++;

        // LCRQ
        results[ic][it] = bench.enqDeq<LCRQueue<UserData,HazardPointers>>               (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<LCRQueue<UserData,PassTheBuck>>                  (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<LCRQueue<UserData,PassThePointer>>               (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<LCRQueueOrcGC<UserData>>                         (cNames[ic], numPairs, cfg.runs);
        ic++;

        // Turn queue (wait-free)
        results[ic][it] = bench.enqDeq<TurnQueue<UserData,HazardPointers>>    (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<TurnQueue<UserData,PassTheBuck>>       (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<TurnQueue<UserData,PassThePointer>>    (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<TurnQueueOrcGC<UserData>>              (cNames[ic], numPairs, cfg.runs);
        ic++;
        /*
        // BitNext lock-free queue
        results[ic][it] = bench.enqDeq<BitNextQueue<UserData,HazardPointers>>         (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextQueue<UserData,PassTheBuck>>            (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextQueue<UserData,PassThePointer>>         (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextQueue<UserData,TagThePointer>>            (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextQueueOrcGC<UserData>>                     (cNames[ic], numPairs, cfg.runs);
        ic++;

        // BitNext LazyHead lock-free queue
        results[ic][it] = bench.enqDeq<BitNextLazyHeadQueue<UserData,HazardPointers>>         (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextLazyHeadQueue<UserData,PassTheBuck>>            (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextLazyHeadQueue<UserData,PassThePointer>>         (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextLazyHeadQueue<UserData,TagThePointer>>            (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<BitNextLazyHeadQueueOrcGC<UserData>>                    (cNames[ic], numPairs, cfg.runs);
        ic++;

        // FAA Array lock-free queue
        results[ic][it] = bench.enqDeq<FAAArrayQueue<UserData,HazardPointers>>           (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<FAAArrayQueue<UserData,PassTheBuck>>              (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<FAAArrayQueue<UserData,PassThePointer>>           (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<FAAArrayQueue<UserData,TagThePointer>>            (cNames[ic], numPairs, cfg.runs);
        ic++;
        results[ic][it] = bench.enqDeq<FAAArrayQueueOrcGC<UserData>>                     (cNames[ic], numPairs, cfg.runs);
        ic++;

        // Kogan-Petrank (wait-free)
        results[ic][it] = bench.enqDeq<KoganPetrankQueueOrcGC<UserData>>              (cNames[ic], numPairs, cfg.runs);
        ic++;
        */

        maxClass = ic;
    }

    if (maxClass == 0) {
        std::cout << "unrecognized command line option...\n";
        return 0;
    }
    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names for each column
    for (int ic = 0; ic < maxClass; ic++) dataFile << cNames[ic] << "\t";
    dataFile << "\n";
    for (int it = 0; it < cfg.threads.size(); it++) {
        dataFile << cfg.threads[it] << "\t";
        for (int ic = 0; ic < maxClass; ic++) dataFile << results[ic][it] << "\t";
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
