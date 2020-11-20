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

// DCAS / CAS2 macro
#define DCAS(ptr, o1, o2, n1, n2)                               \
({                                                              \
    char __ret;                                                 \
    __typeof__(o2) __junk;                                      \
    __typeof__(*(ptr)) __old1 = (o1);                           \
    __typeof__(o2) __old2 = (o2);                               \
    __typeof__(*(ptr)) __new1 = (n1);                           \
    __typeof__(o2) __new2 = (n2);                               \
    asm volatile("lock cmpxchg16b %2;setz %1"                   \
                   : "=d"(__junk), "=a"(__ret), "+m" (*ptr)     \
                   : "b"(__new1), "c"(__new2),                  \
                     "a"(__old1), "d"(__old2));                 \
    __ret; })


/*
 * Pass The Buck
 *
 * Taken from this paper:
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.89.6031&rep=rep1&type=pdf
 * by Maurice Herlihy, Victor Lunchangco and Mark Moir
 *
 * We use double-word-CAS to be able to CAS together a pointer and a version
 *
 */
template<typename T>
class PassTheBuck {

private:
    static const int                HP_MAX_HPS = 16;     // This is named 'K' in the HP paper
    static const int                CLPAD = 128/sizeof(std::atomic<T*>);
    const int                       maxHPs;

    alignas(128) std::atomic<T*>   hp[REGISTRY_MAX_THREADS][HP_MAX_HPS];         // This is named POST[] in the paper
    alignas(128) std::atomic<T*>*   handovers[REGISTRY_MAX_THREADS];  // This is named HANDOFF[] in the paper

    // Internal list, meant to be stack-allocated.
    // We don't use std::vector because we don't want to do allocation.
    struct ValueSet {
        T* set[REGISTRY_MAX_THREADS*HP_MAX_HPS];
        int index {0};

        inline void insert(T* v) {
            set[index] = v;
            index++;
        }

        inline bool search(T* v) {
            for (int i = 0; i < index; i++) if (set[i] == v) return true;
            return false;
        }

        inline void remove(T* v) {
            for (int i = 0; i < index; i++) {
                if (set[i] == v) {
                    for (int j = i; j < index-1; j++) set[j] = set[j+1];
                    index--;
                    return;
                }
            }
        }

        inline void deleteAll() {
            for (int i = 0; i < index; i++) delete set[i];
            index = 0;
        }
    };

    // helper function
    inline void getAtomicH(int it, int ihp, T*& hval, uint64_t& hver) {
        while (true) {
            hver = (uint64_t)(handovers[it][ihp*CLPAD + 1].load());
            hval = handovers[it][ihp*CLPAD].load();
            if (hver == (uint64_t)(handovers[it][ihp*CLPAD + 1].load())) return;
        }
    }

    // Similar to liberate() in the Pass-The-Buck paper, but meant for a single object
    // instead of a set and de-allocates the object if it's not handed off.
    inline void liberate(T* ptr) {
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        ValueSet vs{};
        vs.insert(ptr);
        for (int it = 0; it < maxThreads; it++) {
            for (int ihp = 0; ihp < maxHPs; ihp++) {
                int attempts = 0;
                T* hval;
                uint64_t hver;
                getAtomicH(it, ihp, hval, hver); // Saves into hval and hver the two words taken from handovers[][], in an atomic way
                T* v = hp[it][ihp].load();
                if (v != nullptr && vs.search(v)) {
                    while (true) {
                        if (DCAS((uint64_t*)&handovers[it][ihp*CLPAD], (uint64_t)hval, hver, (uint64_t)v, hver+1)) {
                            vs.remove(v);
                            if (hval != nullptr) vs.insert(hval);
                            break;
                        }
                        attempts++;
                        if (attempts == 3) break;
                        getAtomicH(it, ihp, hval, hver);
                        if (attempts == 2 && hval != nullptr) break;
                        if (v != hp[it][ihp].load()) break;
                    }
                } else {
                    if (hval != nullptr && hval != v) {
                        if (DCAS((uint64_t*)&handovers[it][ihp*CLPAD], (uint64_t)hval, hver, (uint64_t)nullptr, hver+1)) {
                            vs.insert(hval);
                        }
                    }
                }
            }
        }
        vs.deleteAll();
    }

public:
    PassTheBuck(int maxHPs=HP_MAX_HPS) : maxHPs{maxHPs} {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            handovers[it] = new std::atomic<T*>[maxHPs*CLPAD];
            for (int ihp = 0; ihp < maxHPs; ihp++) {
                hp[it][ihp].store(nullptr, std::memory_order_relaxed);
                handovers[it][ihp*CLPAD].store(nullptr, std::memory_order_relaxed);
                handovers[it][ihp*CLPAD + 1].store((T*)0, std::memory_order_relaxed);
            }
        }
    }

    ~PassTheBuck() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            for (int ihp = 0; ihp < maxHPs; ihp++) {
                T* ptr = handovers[it][ihp*CLPAD].load();
                if (ptr != nullptr) delete ptr;
            }
            delete[] handovers[it];
        }
    }

    static std::string className() { return "PassTheBuck"; }

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
    }


    /**
     * Progress Condition: wait-free population oblivious
     */
    inline void clearOne(int ihp) {
        const int tid = ThreadRegistry::getTID();
        hp[tid][ihp].store(nullptr, std::memory_order_release);
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
     * Progress Condition: wait-free bounded (by the number of threads)
     */
    inline void retire(T* ptr) {
        if (ptr == nullptr) return;
        liberate(ptr);
    }
};
