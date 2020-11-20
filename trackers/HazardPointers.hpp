/*
 * Copyright 2020
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once

#include <atomic>
#include <iostream>
#include <vector>
#include "common/ThreadRegistry.hpp"


template<typename T>
class HazardPointers {

private:
    static const int      MAX_HPS = 32;     // This is named 'K' in the HP paper
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HP_THRESHOLD_R = 0; // This is named 'R' in the HP paper
    static const int      MAX_RETIRED = REGISTRY_MAX_THREADS*MAX_HPS; // Maximum number of retired objects per thread

    const int             maxHPs;

    alignas(128) std::atomic<T*>      hp[REGISTRY_MAX_THREADS][MAX_HPS];
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    alignas(128) std::vector<T*>       retiredList[REGISTRY_MAX_THREADS*CLPAD];

public:
    HazardPointers(int maxHPs=MAX_HPS) : maxHPs{maxHPs} {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
        	retiredList[it*CLPAD].reserve(MAX_RETIRED);
            for (int ihp = 0; ihp < MAX_HPS; ihp++) {
                hp[it][ihp].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    ~HazardPointers() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            // Clear the current retired nodes
            for (unsigned iret = 0; iret < retiredList[it*CLPAD].size(); iret++) {
                delete retiredList[it*CLPAD][iret];
            }
        }
    }

    static std::string className() { return "HazardPointers"; }

    // Base class from which T must inherit. Not used here but needed by our benchmarks
    struct BaseObj { };

    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     */
    inline void clear() {
        const int tid = ThreadRegistry::getTID();
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid][ihp].store(nullptr, std::memory_order_release);
        }
    }

    /**
     * Progress Condition: lock-free
     */
    inline T* protect(int index, const std::atomic<T*>* addr) {
        const int tid = ThreadRegistry::getTID();
        T* nptr = nullptr;
        T* aptr;
        while ((aptr = addr->load()) != nptr) {
#ifdef ALWAYS_USE_EXCHANGE
            hp[tid][index].exchange(aptr);
#else
            hp[tid][index].store(aptr);
#endif
            nptr = aptr;
        }
        return aptr;
    }
	
    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    inline T* protectPtr(int index, T* ptr) {
        const int tid = ThreadRegistry::getTID();
#ifdef ALWAYS_USE_EXCHANGE
        hp[tid][index].exchange(ptr);
#else
        hp[tid][index].store(ptr);
#endif
        /*
        // For x86-only implementations, use this instead (it's 2x faster than mfence on x86):
        hp[tid][index].store(ptr, std::memory_order_release);
        __asm__ __volatile__ ("lock;addl $0,(%%rsp);" ::: "cc","memory") ;
        */
        return ptr;
    }

    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    inline T* protectPtrRelease(int index, T* ptr, int other=-1) {
        const int tid = ThreadRegistry::getTID();
        hp[tid][index].store(ptr, std::memory_order_release);
        return ptr;
    }

    inline void swapPtrs(int to, int from) {
        const int tid = ThreadRegistry::getTID();
        T* ptr = hp[tid][from].load();
        hp[tid][from].store(hp[tid][to].load(), std::memory_order_release);
#ifdef ALWAYS_USE_EXCHANGE
        hp[tid][to].exchange(ptr);
#else
        hp[tid][to].store(ptr);
#endif
    }

    /**
     * Progress Condition: wait-free bounded (by the number of threads squared)
     */
    void retire(T* ptr) {
        const int tid = ThreadRegistry::getTID();
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        auto& rlist = retiredList[tid*CLPAD];
        rlist.push_back(ptr);
        if (rlist.size() < HP_THRESHOLD_R) return;
        for (unsigned iret = 0; iret < rlist.size();) {
            auto obj = rlist[iret];
            bool canDelete = true;
            for (int tid = 0; tid < maxThreads && canDelete; tid++) {
                for (int ihp = 0; ihp < maxHPs; ihp++) {
                    if (hp[tid][ihp].load() == obj) {
                        canDelete = false;
                        break;
                    }
                }
            }
            if (canDelete) {
                rlist.erase(rlist.begin() + iret);
                delete obj;
                continue;
            }
            iret++;
        }
    }
};

