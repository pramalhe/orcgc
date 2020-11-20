#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include "BenchmarkSets.hpp"
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"
#include "trackers/PassTheBuck.hpp"
#include "datastructures/lists/MichaelHarrisLinkedListSet.hpp"
#include "datastructures/lists/MichaelHarrisLinkedListSetOrcGC.hpp"
#include "datastructures/lists/HarrisOriginalLinkedListSetOrcGC.hpp"
#include "datastructures/lists/HerlihyShavitHarrisLinkedListSetOrcGC.hpp"
#include "datastructures/lists/TBKPLinkedListSetOrcGC.hpp"


//
// Use like this:
// # bin/set-ll-1k mh-ptp 20 1
//
int main(int argc, char* argv[]) {
    std::string dataFilename { "data/set-ll-1k.txt" };
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64, 96, 128};  // For moleson
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 40, 48, 64, 80 };   // For castor
    //vector<int> threadList = { 1, 2, 4, 8, 16};                        // For laptop
    vector<int> ratioList = { 1000, 100, 0 };                          // Permil ratio: 100%, 10%, 1%, 0%
    const int numElements = 1000;                                      // Number of keys in the set
    // Read the name of data structure from the command line
    char *dsname = (argc >= 2) ? argv[1] : nullptr;
    // Adjust the name of the output file accordingly
    std::string dataFileName;
    if (dsname == nullptr) {
        dataFilename = { "data/set-ll-1k.txt" };
    } else {
        dataFilename = { "data/set-ll-1k-"+std::string{dsname}+".txt" };
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
            std::cout << "\n----- Sets (Linked-Lists)   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
            if (dsname == nullptr || std::strcmp(dsname, "mh-hp") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSet<UserWord,HazardPointers>,UserWord>(cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "mh-ptb") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSet<UserWord,PassTheBuck>,UserWord>               (cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "mh-ptp") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSet<UserWord,PassThePointer>,UserWord>            (cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "mh-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSetOrcGC<UserWord>,UserWord>                      (cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "ho-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<HarrisOriginalLinkedListSetOrcGC<UserWord>,UserWord>                   (cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "hsh-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<HerlihyShavitHarrisLinkedListSetOrcGC<UserWord>,UserWord>              (cNames[ic], ratio, testLength, numRuns, numElements, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "tbkp-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<TBKPLinkedListSetOrcGC<UserWord>,UserWord>                             (cNames[ic], ratio, testLength, numRuns, numElements, false);
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
