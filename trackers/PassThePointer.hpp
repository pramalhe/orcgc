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


/*
 * Pass The Pointer
 *
 * This is a lock-free manual memory reclamation scheme with
 * linear bound on memory usage and without a need for a list of
 * retired objects.
 * Retired objects are either immediately de-allocated (when safe to do so),
 * or they are handed over to one of the threads that has a published pointer
 * for that object.
 * One handover slot exits per hazardous pointer. If the slot of another thread
 * is already taken, we take the object that is there with an exchange() (or a CAS())
 * and replace it with the new one. We then scan for a published pointer matching
 * that older object and repeat the process, either handing over the object to
 * another thread or de-allocating it if it is safe to do so.
 *
 * Unlike the HP LB, this algorithm does not execute de-allocation on the protect()
 * call, nor does it execute a fence on the protectPtr() call.
 *
 * The total bound on memory usage is precisely the number of hazardous pointers
 * times the number of threads.
 *
 */
template<typename T>
class PassThePointer {

private:
    static const int                HP_MAX_HPS = 32;     // This is named 'K' in the HP paper
    const int                       maxHPs;

    alignas(128) std::atomic<T*>    hp[REGISTRY_MAX_THREADS][HP_MAX_HPS];
    alignas(128) std::atomic<T*>    handovers[REGISTRY_MAX_THREADS][HP_MAX_HPS];

    // Tries to handover an object to another thread which may be still using it.
    // If no other thread is using this object, de-allocate it.
    // Progress condition: Lock-free
    inline void handoverOrDelete(T* ptr, int start, const int maxThreads) {
        const int tid = ThreadRegistry::getTID();
        if (ptr == nullptr) return;
        for (int it = start; it < maxThreads; it++) {
            for (int ihp = 0; ihp < maxHPs; ) {
                // TODO: We may want to deal with the case where the hp.load() changes at the
                // same time as handovers.exchange(). Maybe return handovers.exchange(nullptr).
                // Notice it is not needed for correctness or memory bound, but it would be
                // a more robust design in case the thread goes away to do other stuff.
                if (hp[it][ihp].load() == ptr) {
                    // Thread 'it' is using 'ptr': hand it over to that thread
                    ptr = handovers[it][ihp].exchange(ptr);
                    // Notice we don't restart because if ptr is non-null, it was handed
                    // over by a thread that scanned (in the same order) up until this entry.
                    if (ptr == nullptr) return;
                    // We need to re-scan just to check that it's not the new ptr
                    if (hp[it][ihp].load() == ptr) continue;
                }
                ihp++;
            }
        }
        delete ptr;
    }

public:
    PassThePointer(int maxHPs=HP_MAX_HPS) : maxHPs{maxHPs} {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            for (int ihp = 0; ihp < maxHPs; ihp++) {
                hp[it][ihp].store(nullptr, std::memory_order_relaxed);
                handovers[it][ihp].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    ~PassThePointer() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            for (int ihp = 0; ihp < maxHPs; ihp++) {
                T* ptr = handovers[it][ihp].load();
                if (ptr != nullptr) delete ptr;
            }
        }
    }

    static std::string className() { return "PassThePointer"; }

    // Base class from which T must inherit. Not used here but need by our benchmarks
    struct BaseObj { };

    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     */
    inline void clear() {
        const int tid = ThreadRegistry::getTID();
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid][ihp].store(nullptr, std::memory_order_release);
        }
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            if (handovers[tid][ihp].load() != nullptr) {
                T* ptr = handovers[tid][ihp].exchange(nullptr);
                if (ptr != nullptr) handoverOrDelete(ptr, tid, maxThreads);
            }
        }
    }


    /**
     * Progress Condition: wait-free population oblivious
     */
    inline void clearOne(int ihp) {
        const int tid = ThreadRegistry::getTID();
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        hp[tid][ihp].store(nullptr, std::memory_order_release);
        if (handovers[tid][ihp].load() != nullptr) {
            T* ptr = handovers[tid][ihp].exchange(nullptr);
            if (ptr != nullptr) handoverOrDelete(ptr, tid, maxThreads);
        }
    }

    // Progress Condition: lock-free
    inline T* protect(int index, const std::atomic<T*>* addr) {
        const int tid = ThreadRegistry::getTID();
        T *pub, *ptr = nullptr;
        while ((pub = addr->load()) != ptr) {
#ifdef ALWAYS_USE_EXCHANGE
            hp[tid][index].exchange(pub);
#else
            hp[tid][index].store(pub);
#endif
            ptr = pub;
        }
        return pub;
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
     * Progress Condition: wait-free bounded (by the number of threads)
     */
    inline void retire(T* ptr) {
        if (ptr == nullptr) return;
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        handoverOrDelete(ptr, 0, maxThreads);
    }
};
