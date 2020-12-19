#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include "BenchmarkSets.hpp"
#include "common/CmdLineConfig.hpp"
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"
#include "trackers/PassTheBuck.hpp"
#include "datastructures/skiplists/HerlihyShavitLockFreeSkipListOrcGC.hpp"
#include "datastructures/skiplists/HerlihyShavitLockFreeSkipListOrcGCOrig.hpp"


//
// Use like this:
// # bin/set-ll-1k mh-ptp --duration=20 --threads=1,2,4
//
int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    // Adjust the name of the output file accordingly
    std::string dataFilename;
    // Read the name of data structure from the command line
    char *dsname = (argc >= 2) ? argv[1] : nullptr;
    if (dsname == nullptr) {
        dataFilename = { "data/set-skiplist-1m.txt" };
    } else {
        dataFilename = { "data/set-skiplist-1m-"+std::string{dsname}+".txt" };
    }
    seconds testLength {cfg.duration};
    const int EMAX_CLASS = 30;
    uint64_t results[EMAX_CLASS][cfg.threads.size()][cfg.ratios.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*cfg.threads.size()*cfg.ratios.size());

    std::cout << "This benchmark is going to take at most " << cfg.computeTotalHours() << " hours to complete\n";

    for (unsigned ir = 0; ir < cfg.ratios.size(); ir++) {
        auto ratio = cfg.ratios[ir];
        for (unsigned it = 0; it < cfg.threads.size(); it++) {
            auto nThreads = cfg.threads[it];
            int ic = 0;
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (Skiplist)   numkeys=" << cfg.keys << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << cfg.runs << "   length=" << testLength.count() << "s -----\n";
            if (dsname == nullptr || std::strcmp(dsname, "hsskip-orcorig") == 0) {
				results[ic][it][ir] = bench.benchmark<HerlihyShavitLockFreeSkipListOrcGCOrig<UserWord>,UserWord> (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
				ic++;
			}
            if (dsname == nullptr || std::strcmp(dsname, "hsskip-orc") == 0) {
				results[ic][it][ir] = bench.benchmark<HerlihyShavitLockFreeSkipListOrcGC<UserWord>,UserWord>     (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
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
    for (unsigned iratio = 0; iratio < cfg.ratios.size(); iratio++) {
        auto ratio = cfg.ratios[iratio];
        for (int iclass = 0; iclass < maxClass; iclass++) dataFile << cNames[iclass] << "-" << ratio/10. << "%"<< "\t";
    }
    dataFile << "\n";
    for (int it = 0; it < cfg.threads.size(); it++) {
        dataFile << cfg.threads[it] << "\t";
        for (unsigned ir = 0; ir < cfg.ratios.size(); ir++) {
            for (int ic = 0; ic < maxClass; ic++) dataFile << results[ic][it][ir] << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
