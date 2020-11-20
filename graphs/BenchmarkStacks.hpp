/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>


using namespace std;
using namespace chrono;

struct UserData  {
    long long seq;
    int tid;
    UserData(long long lseq, int ltid) {
        this->seq = lseq;
        this->tid = ltid;
    }
    UserData() {
        this->seq = -2;
        this->tid = -2;
    }
    UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

    bool operator < (const UserData& other) const {
        return seq < other.seq;
    }
};


/**
 * This is a micro-benchmark to run the tests shown in CRTurnQueue paper
 *
 * <h2> Performance Benchmarks </h2>
 * TODO
 *
 *
 * <h2> Latency Distribution </h2>
 *
 * - We fire up 28 threads of type LatencyThread;
 * - Each thread does alternatively 1000 enqueue() and 1000 dequeue(). All dequeues are non-null;
 * - After start, each thread does 1M iterations as warmup.
 * - Measurements are done for 4M iterations, that are saved in a local array, 2M enqueue and 2M dequeue;
 * -
 *
 */
class BenchmarkStacks {

private:

    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };

    // Performance benchmark constants
    static const long long kNumPairsWarmup =     1000000LL;     // Each threads does 1M iterations as warmup

    // Contants for Ping-Pong performance benchmark
    static const int kPingPongBatch = 1000;            // Each thread starts by injecting 1k items in the queue


    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:

    BenchmarkStacks(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * enqueue-dequeue pairs: in each iteration a thread executes an enqueue followed by a dequeue;
     * the benchmark executes 10^8 pairs partitioned evenly among all threads;
     */
    template<typename S>
    uint64_t pushPop(std::string& className, const long numPairs, const int numRuns) {
        nanoseconds deltas[numThreads][numRuns];
        atomic<bool> startFlag = { false };
        S* stack = nullptr;
        className = S::className();
        cout << "##### " << className << " #####  \n";

        auto enqdeq_lambda = [this,&startFlag,&numPairs,&stack](nanoseconds *delta, const int tid) {
            UserData ud(0,0);
            while (!startFlag.load()) {} // Spin until the startFlag is set
            // Warmup phase
            for (long long iter = 0; iter < kNumPairsWarmup/numThreads; iter++) {
                stack->push(&ud);
                if (stack->pop() == nullptr) cout << "Error at warmup dequeueing iter=" << iter << "\n";
            }
            // Measurement phase
            auto startBeats = steady_clock::now();
            for (long long iter = 0; iter < numPairs/numThreads; iter++) {
                stack->push(&ud);
                if (stack->pop() == nullptr) cout << "Error at measurement dequeueing iter=" << iter << "\n";
            }
            auto stopBeats = steady_clock::now();
            *delta = stopBeats - startBeats;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            stack = new S();
            thread enqdeqThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid] = thread(enqdeq_lambda, &deltas[tid][irun], tid);
            startFlag.store(true);
            // Sleep for 2 seconds just to let the threads see the startFlag
            this_thread::sleep_for(2s);
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid].join();
            startFlag.store(false);
            delete (S*)stack;
        }

        // Sum up all the time deltas of all threads so we can find the median run
        vector<nanoseconds> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            agg[irun] = 0ns;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += deltas[tid][irun];
            }
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        auto median = agg[numRuns/2].count()/numThreads; // Normalize back to per-thread time (mean of time for this run)

        cout << "Total Ops/sec = " << numPairs*2*NSEC_IN_SEC/median << "\n";
        return (numPairs*2*NSEC_IN_SEC/median);
    }
};
