#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include "BenchmarkSets.hpp"
#include "common/CmdLineConfig.hpp"
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"
#include "trackers/PassTheBuck.hpp"
#include "datastructures/lists/MichaelHarrisLinkedListSet.hpp"
#include "datastructures/lists/MichaelHarrisLinkedListSetOrcGC.hpp"
#include "datastructures/lists/HarrisOriginalLinkedListSetOrcGC.hpp"
#include "datastructures/lists/HerlihyShavitHarrisLinkedListSetOrcGC.hpp"
#include "datastructures/lists/TBKPLinkedListSetOrcGC.hpp"
//#include "datastructures/trees/EFRBBSTMap.hpp"


//
// Use like this:
// # bin/set-ll-1k mh-ptp 20 1
//
int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    std::string dataFilename { "data/set-ll-1k.txt" };
    // Adjust the name of the output file accordingly
    std::string dataFileName;
    // Read the name of data structure from the command line
    char *dsname = (argc >= 2) ? argv[1] : nullptr;
    if (dsname == nullptr) {
        dataFilename = { "data/set-ll-1k.txt" };
    } else {
        dataFilename = { "data/set-ll-1k-"+std::string{dsname}+".txt" };
    }
    seconds testLength {cfg.duration};
    const int EMAX_CLASS = 30;
    uint64_t results[EMAX_CLASS][cfg.threads.size()][cfg.ratios.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*cfg.threads.size()*cfg.ratios.size());

    double totalHours = (double)EMAX_CLASS*cfg.ratios.size()*cfg.threads.size()*testLength.count()*cfg.runs/(60.*60.);
    std::cout << "This benchmark is going to take at most " << totalHours << " hours to complete\n";

    for (unsigned ir = 0; ir < cfg.ratios.size(); ir++) {
        auto ratio = cfg.ratios[ir];
        for (unsigned it = 0; it < cfg.threads.size(); it++) {
            auto nThreads = cfg.threads[it];
            int ic = 0;
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (Linked-Lists)   numKeys=" << cfg.keys << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << cfg.runs << "   length=" << testLength.count() << "s -----\n";
            if (dsname == nullptr || std::strcmp(dsname, "mh-hp") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSet<UserWord,HazardPointers>,UserWord>            (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "mh-ptb") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSet<UserWord,PassTheBuck>,UserWord>               (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "mh-ptp") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSet<UserWord,PassThePointer>,UserWord>            (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "mh-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<MichaelHarrisLinkedListSetOrcGC<UserWord>,UserWord>                      (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "ho-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<HarrisOriginalLinkedListSetOrcGC<UserWord>,UserWord>                     (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "hsh-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<HerlihyShavitHarrisLinkedListSetOrcGC<UserWord>,UserWord>                (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "tbkp-orc") == 0) {
                results[ic][it][ir] = bench.benchmark<TBKPLinkedListSetOrcGC<UserWord>,UserWord>                               (cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
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
