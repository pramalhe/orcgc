#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#include "BenchmarkSets.hpp"
#include "common/CmdLineConfig.hpp"
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"
#include "trackers/PassTheBuck.hpp"
#include "datastructures/trees/NatarajanTreeOrcGC.hpp"
#include "datastructures/trees/NatarajanTree.hpp"


int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    std::string dataFilename { "data/set-tree-1m.txt" };
    // Read the name of data structure from the command line
    char *dsname = (argc >= 2) ? argv[1] : nullptr;
    // Adjust the name of the output file accordingly
    std::string dataFileName;
    if (dsname == nullptr) {
        dataFilename = { "data/set-tree-1m.txt" };
    } else {
        dataFilename = { "data/set-tree-1m-"+std::string{dsname}+".txt" };
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
            std::cout << "\n----- Sets (Trees)   numKeys=" << cfg.keys << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << cfg.runs << "   length=" << testLength.count() << "s -----\n";
            //if (dsname == nullptr || std::strcmp(dsname, "efrb-orc") == 0) {
            //    results[ic][it][ir] = bench.benchmarkRandomFill<EFRBBSTMapOrcGC<UserWord,UserWord>,UserWord>(cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
            //    ic++;
            //}
            if (dsname == nullptr || std::strcmp(dsname, "nata-hp") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTree<uint64_t,uint64_t,HazardPointers>,uint64_t>(cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "nata-ptb") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTree<uint64_t,uint64_t,PassTheBuck>,uint64_t>(cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "nata-ptp") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTree<uint64_t,uint64_t,PassThePointer>,uint64_t>(cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
                ic++;
            }
            if (dsname == nullptr || std::strcmp(dsname, "nata-orc") == 0) {
                results[ic][it][ir] = bench.benchmarkRandomFill<NatarajanTreeOrcGC<uint64_t,uint64_t>,uint64_t>(cNames[ic], ratio, testLength, cfg.runs, cfg.keys, false);
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
