#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include "BenchmarkSets.hpp"
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"
#include "trackers/PassTheBuck.hpp"
#include "datastructures/trees/NatarajanTreeOrcGC.hpp"
#include "datastructures/trees/NatarajanTree.hpp"


int main(int argc, char* argv[]) {
    std::string dataFilename { "data/set-tree-1m.txt" };
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64, 96, 128 };  // For moleson
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 40, 48, 64, 80 };   // For castor
    //vector<int> threadList = { 1, 2, 4, 8, 16};                        // For laptop
    vector<int> ratioList = { 1000, 100, 0 };                          // Permil ratio: 100%, 10%, 1%, 0%
    const int numElements = 1000*1000;                                      // Number of keys in the set          TODO: CHANGE THIS TO 1M
    // Read the name of data structure from the command line
    char *dsname = (argc >= 2) ? argv[1] : nullptr;
    // Adjust the name of the output file accordingly
    std::string dataFileName;
    if (dsname == nullptr) {
        dataFilename = { "data/set-tree-1m.txt" };
    } else {
        dataFilename = { "data/set-tree-1m-"+std::string{dsname}+".txt" };
    }
    // Read the number of seconds from the command line or use 4 seconds as default
    long secs = (argc >= 3) ? atoi(argv[2]) : 4;
    seconds testLength {secs};
    // Read the number of runs from command line or use 1 run as default
    const int numRuns = (argc >= 4) ? atoi(argv[3]) : 1;
    const int EMAX_CLASS = 30;
    uint64_t results[EMAX_CLASS][threadList.size()][ratioList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size()*ratioList.size());

    double totalHours = (double)EMAX_CLASS*ratioList.size()*threadList.size()*testLength.count()*numRuns/(60.*60.);
    std::cout << "This benchmark is going to take at most " << totalHours << " hours to complete\n";

    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        for (unsigned it = 0; it < threadList.size(); it++) {
            auto nThreads = threadList[it];
            int ic = 0;
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (Trees)   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";

            if (dsname == nullptr || std::strcmp(dsname, "nata-hp") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTree<uint64_t,uint64_t,HazardPointers>,uint64_t>(cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "nata-ptb") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTree<uint64_t,uint64_t,PassTheBuck>,uint64_t>(cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "nata-ptp") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTree<uint64_t,uint64_t,PassThePointer>,uint64_t>(cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "nata-orc") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTreeOrcGC<uint64_t,uint64_t>,uint64_t>(cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }

            maxClass = ic;
        }
    }

    if (maxClass == 0) {
        std::cout << "unrecognized command line option...\n";
        return 0;
    }
    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names and ratios for each column
    for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
        auto ratio = ratioList[iratio];
        for (int iclass = 0; iclass < maxClass; iclass++) dataFile << cNames[iclass] << "-" << ratio/10. << "%"<< "\t";
    }
    dataFile << "\n";
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (unsigned ir = 0; ir < ratioList.size(); ir++) {
            for (int ic = 0; ic < maxClass; ic++) dataFile << results[ic][it][ir] << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
