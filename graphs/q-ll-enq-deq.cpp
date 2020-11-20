/*
 * Executes the following non-blocking (linked list based) queues in a single-enqueue-single-dequeue benchmark:
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "BenchmarkQueues.hpp"
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


#define MILLION  1000000LL

int main(void) {
    const std::string dataFilename { "data/q-ll.txt" };
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64, 96, 128 };  // For moleson
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 40, 48, 64, 80 };   // For castor
    const int numRuns = 5;                                             // Number of runs
    const long numPairs = 10*MILLION;                                  // 10M is fast enough on the laptop, but on AWS we can use 100M
    const int EMAX_CLASS = 100;
    uint64_t results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    // Enq-Deq Throughput benchmarks
    for (int it = 0; it < threadList.size(); it++) {
        int nThreads = threadList[it];
        int ic = 0;
        BenchmarkQueues bench(nThreads);
        std::cout << "\n----- q-ll-enq-deq   threads=" << nThreads << "   pairs=" << numPairs/MILLION << "M   runs=" << numRuns << " -----\n";

        // Maged Michael and Michael Scott's lock-free queue
        results[ic][it] = bench.enqDeq<MichaelScottQueue<UserData,HazardPointers>>    (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<MichaelScottQueue<UserData,PassTheBuck>>       (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<MichaelScottQueue<UserData,PassThePointer>>    (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<MichaelScottQueueOrcGC<UserData>>              (cNames[ic], numPairs, numRuns);
        ic++;

        // LCRQ
        results[ic][it] = bench.enqDeq<LCRQueue<UserData,HazardPointers>>               (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<LCRQueue<UserData,PassTheBuck>>                  (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<LCRQueue<UserData,PassThePointer>>               (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<LCRQueueOrcGC<UserData>>                         (cNames[ic], numPairs, numRuns);
        ic++;

        // Turn queue (wait-free)
        results[ic][it] = bench.enqDeq<TurnQueue<UserData,HazardPointers>>    (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<TurnQueue<UserData,PassTheBuck>>       (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<TurnQueue<UserData,PassThePointer>>    (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<TurnQueueOrcGC<UserData>>              (cNames[ic], numPairs, numRuns);
        ic++;
        /*

        // Kogan-Petrank (wait-free)
        results[ic][it] = bench.enqDeq<KoganPetrankQueueOrcGC<UserData>>              (cNames[ic], numPairs, numRuns);
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
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (int ic = 0; ic < maxClass; ic++) dataFile << results[ic][it] << "\t";
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
