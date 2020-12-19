/*
 * Copyright 2020
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <cstdint>
#include <cstring>


// This class stores and parses the workload configuration parameters
struct CmdLineConfig {
    uint64_t keys             {1000};                         // Number of keys
    uint64_t duration           {2};                          // Duration of the benchmark (in seconds)
    uint64_t runs               {1};                          // Number of runs
    std::vector<int> threads = {1,2,4,8,10,16,20,24,32,40};   // List of threads
    std::vector<int> ratios = {1000,100,10};                  // List of ratios (in permil)

    CmdLineConfig() {
    }

    // Returns true if the command line arguments where sucessfully parsed
    bool parseCmdLine(int argc, char* argv[]) {
        for (int iarg = 2; iarg < argc; iarg++) {
            if (strcmp("help",argv[iarg]) == 0 || strcmp("--help",argv[iarg]) == 0) {
                printf("Available options:\n");
                printf("--help               This message\n");
                printf("--keys=1000          Number of keys, default is 1000\n");
                printf("--duration=2         Duration of each run in seconds\n");
                printf("--runs=1             Number of runs. Result is the median of all runs\n");
                printf("--threads=1,2,4      Comma separated values with the number of threads\n");
                printf("--ratios=1000,100,0  Comma separated ratios (1000=100%% writes, 100=10%% writes and 90%% reads)\n");
                return false;
            }
            //printf("this: [%s]\n", strstr(argv[iarg], "--num="));
            if (strstr(argv[iarg], "--keys=") != NULL) {
                char* args = strtok(argv[iarg], "--keys=");
                keys = atoi(args);
                continue;
            }
            if (strstr(argv[iarg], "--duration=") != NULL) {
                char* args = strtok(argv[iarg], "--duration=");
                duration = atoi(args);
                continue;
            }
            if (strstr(argv[iarg], "--runs=") != NULL) {
                char* args = strtok(argv[iarg], "--runs=");
                runs = atoi(args);
                continue;
            }
            if (strstr(argv[iarg], "--threads=") != NULL) {
                threads.clear();
                char* args = argv[iarg]+strlen("--threads=");
                args = strtok(args, ",");
                while (args != NULL) {
                    threads.push_back(atoi(args));
                    args = strtok(NULL, ",");
                }
                continue;
            }
            if (strstr(argv[iarg], "--ratios=") != NULL) {
                ratios.clear();
                char* args = argv[iarg]+strlen("--ratios=");
                args = strtok(args, ",");
                while (args != NULL) {
                    ratios.push_back(atoi(args));
                    args = strtok(NULL, ",");
                }
                continue;
            }
            printf("Unknow configuration parameter: [%s]\n", argv[iarg]);
        }

        return true;
    }

    void print() {
        printf("Configuration: num=%ld  duration=%ld  runs=%ld  ", keys, duration, runs);
        printf("threads=");
        for (int i = 0; i < threads.size(); i++) {
            printf("%d,", threads[i]);
        }
        printf("  ratios=");
        for (int i = 0; i < ratios.size(); i++) {
            printf("%.1f%%,", (float)ratios[i]/10.);
        }
        printf("\n");
    }

    // Returns the total number of hours this benchmark will take to execute (not counting for filling up data structures)
    double computeTotalHours() {
        return (double)duration*runs*threads.size()*ratios.size()/(60.*60.);
    }
};
