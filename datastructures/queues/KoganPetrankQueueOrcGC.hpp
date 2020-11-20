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
#include <stdexcept>

#include "../../trackers/OrcPTP.hpp"

using namespace orcgc_ptp;


/**
 * <h1> Kogan-Petrank Queue </h1>
 *
 * This queue does not work with HP/HE/PTB/PTP. It needs something like Conditional Hazard Pointers, or OrcGC
 *
 * http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf
 *
 * enqueue algorithm: Kogan-Petrank, based on the consensus of Lamport's bakery
 * dequeue algorithm: Kogan-Petrank, based on the consensus of Lamport's bakery
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * Memory Reclamation: OrcGC
 *
 */
template<typename T>
class KoganPetrankQueueOrcGC {

private:

    struct Node : public orc_base {
        T*                value;
        orc_atomic<Node*> next { nullptr };
        const int         enqTid;
        std::atomic<int>  deqTid { IDX_NONE };

        Node(T* userItem, int enqTid) : value{userItem}, enqTid{enqTid} { }

        bool casNext(Node* cmp, Node* val) {
            // Use a tmp variable because this CAS "replaces" the value of the first argument
            Node* tmp = cmp;
            return next.compare_exchange_strong(tmp, val);
        }
        void poisonAllLinks() { next.poison(); }
    } __attribute__((aligned(128)));


    struct OpDesc : public orc_base {
        const long long   phase;
        const bool        pending;
        const bool        enqueue;
        orc_atomic<Node*> node; // This is immutable once assigned, but we need to use orc_atomic to keep track of references
        OpDesc (long long ph, bool pend, bool enq, Node* n) : phase{ph}, pending{pend}, enqueue{enq}, node{n} { }
        void poisonAllLinks() { node.poison(); }
    } __attribute__((aligned(128)));


    bool casTail(Node *cmp, Node *val) {
        return tail.compare_exchange_strong(cmp, val);
    }

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // Member variables

    // Pointers to head and tail of the list
    alignas(128) orc_atomic<Node*> head {nullptr};
    alignas(128) orc_atomic<Node*> tail {nullptr};
    // Array of enque and dequeue requests
    alignas(128) orc_atomic<OpDesc*> state[REGISTRY_MAX_THREADS];

    const static long long IDX_NONE = -1;


public:
    KoganPetrankQueueOrcGC() {
    	orc_ptr<Node*> sentinelNode = make_orc<Node>(nullptr, -1);
        head = sentinelNode;
        tail = sentinelNode;
        orc_ptr<OpDesc*> OPDESC_END = make_orc<OpDesc>(-1,  false, true, nullptr);
        for (int i = 0; i < REGISTRY_MAX_THREADS; i++) state[i] = OPDESC_END;
    }

    ~KoganPetrankQueueOrcGC() {
        while (dequeue() != nullptr); // Drain the queue
        head = nullptr;
        tail = nullptr;
        for (int i = 0; i < REGISTRY_MAX_THREADS; i++) state[i] = nullptr;
    }

    static std::string className() { return "KoganPetrankQueue-OrcGC"; }


    void help(long long phase) {
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        for (int i = 0; i < maxThreads; i++) {
            orc_ptr<OpDesc*> desc = state[i].load();
            if (desc->pending && desc->phase <= phase) {
            	if (desc->enqueue) {
            		help_enq(i, phase);
            	} else {
            		help_deq(i, phase);
            	}
            }
        }
    }


    /**
     * Progress Condition: wait-free bounded by maxThreads
     */
    long long maxPhase() {
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        long long maxPhase = -1;
        for (int i = 0; i < maxThreads; i++) {
            orc_ptr<OpDesc*> desc = state[i].load();
            long long phase = desc->phase;
            if (phase > maxPhase) {
            	maxPhase = phase;
            }
        }
        return maxPhase;
    }


    bool isStillPending(int otid, long long ph) {
        orc_ptr<OpDesc*> desc = state[otid].load();
        return desc->pending && desc->phase <= ph;
    }


    void enqueue(T* item) {
        const int tid = ThreadRegistry::getTID();
        // We better have consecutive thread ids, otherwise this will blow up
        long long phase = maxPhase() + 1;
        state[tid] = make_orc<OpDesc>(phase, true, true, make_orc<Node>(item, tid));
        help(phase);
        help_finish_enq();
    }


    void help_enq(int otid, long long phase) {
        while (isStillPending(otid, phase)) {
            orc_ptr<Node*> last = tail.load();
            orc_ptr<Node*> next = last->next.load();
            if (last == tail) {
                if (next == nullptr) {
                    if (isStillPending(otid, phase)) {
                        orc_ptr<OpDesc*> curDesc = state[otid].load();
                        if (last->casNext(next, curDesc->node)) {
                            help_finish_enq();
                            return;
                        }
                    }
                } else {
                    help_finish_enq();
                }
            }
        }
    }


    void help_finish_enq() {
        orc_ptr<Node*> last = tail.load();
        // The inner loop will run at most twice, because last->next is immutable when non-null
        orc_ptr<Node*> next = last->next.load();
        // Check "last" equals "tail" to prevent ABA on "last->next"
        if (next != nullptr) {
            int otid = next->enqTid;
            orc_ptr<OpDesc*> curDesc = state[otid].load();
            if (last == tail.load() && curDesc->node.load() == next) {
            	orc_ptr<OpDesc*> newDesc = make_orc<OpDesc>(curDesc->phase, false, true, next);
            	state[otid].compare_exchange_strong(curDesc, newDesc);
            	casTail(last, next);
            }
        }
    }


    T* dequeue() {
        const int tid = ThreadRegistry::getTID();
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        // We better have consecutive thread ids, otherwise this will blow up
        long long phase = maxPhase() + 1;
        state[tid] = make_orc<OpDesc>(phase, true, false, nullptr);
        help(phase);
        help_finish_deq();
        orc_ptr<OpDesc*> curDesc = state[tid].load();
        orc_ptr<Node*> node = curDesc->node.load();
        if (node == nullptr) {
            return nullptr; // We return null instead of throwing an exception
        }
        orc_ptr<Node*> next = node->next.load();
        return next->value;
    }


    void help_deq(int otid, long long phase) {
        while (isStillPending(otid, phase)) {
            orc_ptr<Node*> first = head.load();
            orc_ptr<Node*> last = tail.load();
            orc_ptr<Node*> next = first->next.load();
            if (first == head) {
            	if (first == last) {
            		if (next == nullptr) {
            		    orc_ptr<OpDesc*> curDesc = state[otid].load();
            			if (last == tail.load() && isStillPending(otid, phase)) {
            			    orc_ptr<OpDesc*> newDesc = make_orc<OpDesc>(curDesc->phase, false, false, nullptr);
                            state[otid].compare_exchange_strong(curDesc, newDesc);
            			}
                    } else {
                        help_finish_enq();
                    }
                } else {
                    orc_ptr<OpDesc*> curDesc = state[otid].load();
                    orc_ptr<Node*> node = curDesc->node.load();
                    if (!isStillPending(otid, phase)) break;
                    if (first == head.load() && node != first) {
                        orc_ptr<OpDesc*> newDesc = make_orc<OpDesc>(curDesc->phase, true, false, first);
                        if (!state[otid].compare_exchange_strong(curDesc, newDesc)) continue;
                    }
                    int tmp = IDX_NONE;
                    first->deqTid.compare_exchange_strong(tmp, otid);
                    help_finish_deq();
                }
            }
        }
    }


    void help_finish_deq() {
        orc_ptr<Node*> first = head.load();
        orc_ptr<Node*> next = first->next.load();
        int otid = first->deqTid.load();
        if (otid != IDX_NONE) {
            orc_ptr<OpDesc*> curDesc = state[otid].load();
            if (first == head.load() && next != nullptr) {
                orc_ptr<OpDesc*> newDesc = make_orc<OpDesc>(curDesc->phase, false, false, curDesc->node);
            	state[otid].compare_exchange_strong(curDesc, newDesc);
            	casHead(first, next);
            }
        }
    }
};

