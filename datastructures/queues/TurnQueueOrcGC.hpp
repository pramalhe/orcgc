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
 * <h1> Turn Queue </h1>
 *
 * A concurrent wait-free queue that is Multi-Producer-Multi-Consumer and does
 * its own wait-free memory reclamation.
 * Based on the paper "A Wait-Free Queue with Wait-Free Memory Reclamation"
 * https://github.com/pramalhe/ConcurrencyFreaks/tree/master/papers/crturnqueue-2016.pdf
 *
 * <p>
 * Enqueue algorithm: Turn enqueue
 * Dequeue algorithm: Turn dequeue
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * Memory Reclamation: OrcGC (wait-free)
 *
 */
template<typename T>
class TurnQueueOrcGC {

private:
    struct Node : orc_base {
        T* item;
        const int enqTid;
        std::atomic<int> deqTid;
        orc_atomic<Node*> next {nullptr};

        Node(T* item, int tid) : item{item}, enqTid{tid}, deqTid{IDX_NONE} { }

        bool casDeqTid(int cmp, int val) {
     	    return deqTid.compare_exchange_strong(cmp, val);
        }
        void poisonAllLinks() { next.poison(); }
    } __attribute__((aligned(128)));

    static const int IDX_NONE = -1;

    // Pointers to head and tail of the list
    alignas(128) orc_atomic<Node*> head {nullptr};
    alignas(128) orc_atomic<Node*> tail {nullptr};
    // Enqueue requests
    alignas(128) orc_atomic<Node*> enqueuers[REGISTRY_MAX_THREADS];
    // Dequeue requests
    alignas(128) std::atomic<Node*> deqself[REGISTRY_MAX_THREADS];
    alignas(128) orc_atomic<Node*> deqhelp[REGISTRY_MAX_THREADS];





    /**
     * Called only from dequeue()
     *
     * Search for the next request to dequeue and assign it to lnext.deqTid
     * It is only a request to dequeue if deqself[i] equals deqhelp[i].
     */
    int searchNext(orc_ptr<Node*>& lhead, orc_ptr<Node*>& lnext) {
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        const int turn = lhead->deqTid.load();
        for (int idx=turn+1; idx < turn+maxThreads+1; idx++) {
            const int idDeq = idx%maxThreads;
            if (deqself[idDeq].load() != deqhelp[idDeq].load().ptr) continue;
            if (lnext->deqTid.load() == IDX_NONE) lnext->casDeqTid(IDX_NONE, idDeq);
            break;
        }
        return lnext->deqTid.load();
    }


    /**
     * Called only from dequeue()
     *
     * If the ldeqTid is not our own, we must use an HP to protect against
     * deqhelp[ldeqTid] being retired-deleted-newed-reenqueued.
     */
    void casDeqAndHead(orc_ptr<Node*>& lhead, orc_ptr<Node*>& lnext, const int tid) {
        const int ldeqTid = lnext->deqTid.load();
        if (ldeqTid == tid) {
            deqhelp[ldeqTid].store(lnext, std::memory_order_release);
        } else {
            orc_ptr<Node*> ldeqhelp = deqhelp[ldeqTid].load();
            if (ldeqhelp != lnext && lhead == head.load().ptr) {
                deqhelp[ldeqTid].compare_exchange_strong(ldeqhelp, lnext); // Assign next to request
            }
        }
        head.compare_exchange_strong(lhead, lnext);
    }


    /**
     * Called only from dequeue()
     *
     * Giveup procedure, for when there are no nodes left to dequeue
     */
    void giveUp(Node* myReq, const int tid) {
        orc_ptr<Node*> lhead = head.load();
        if (deqhelp[tid].load().ptr != myReq || lhead == tail.load().ptr) return;
        orc_ptr<Node*> lnext = lhead->next.load();
        if (lhead != head.load().ptr) return;
        if (searchNext(lhead, lnext) == IDX_NONE) lnext->casDeqTid(IDX_NONE, tid);
        casDeqAndHead(lhead, lnext, tid);
    }

public:
    TurnQueueOrcGC() {
    	orc_ptr<Node*> sentinelNode = make_orc<Node>(nullptr, 0);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
        for (int i = 0; i < REGISTRY_MAX_THREADS; i++) {
            enqueuers[i].store(nullptr, std::memory_order_relaxed);
            // deqself[i] != deqhelp[i] means that isRequest=false
            deqself[i].store(nullptr, std::memory_order_relaxed);
            deqhelp[i].store(make_orc<Node>(nullptr, 0), std::memory_order_relaxed);
        }
    }


    ~TurnQueueOrcGC() {
        while (dequeue() != nullptr); // Drain the queue
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) deqself[i] = nullptr;
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) deqhelp[i] = nullptr;
    }


    static std::string className() { return "TurnQueue-OrcGC"; }


    /**
     * Steps when uncontended:
     * 1. Add node to enqueuers[]
     * 2. Insert node in tail.next using a CAS
     * 3. Advance tail to tail.next
     * 4. Remove node from enqueuers[]
     *
     * @param tid The tid must be a UNIQUE index for each thread, in the range 0 to maxThreads-1
     */
    void enqueue(T* item) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        const int tid = ThreadRegistry::getTID();
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        enqueuers[tid].store(make_orc<Node>(item,tid));
        orc_ptr<Node*> ltail;
        orc_ptr<Node*> nodeToHelp;
        for (int i = 0; i < maxThreads; i++) {
            if (enqueuers[tid].load() == nullptr) return; // Some thread did all the steps
            ltail = tail.load();
            if (enqueuers[ltail->enqTid].load() == ltail) {  // Help a thread do step 4
                Node* tmp = ltail;
                enqueuers[ltail->enqTid].compare_exchange_strong(tmp, nullptr);
            }
            for (int j = 1; j < maxThreads+1; j++) {         // Help a thread do step 2
                nodeToHelp = enqueuers[(j + ltail->enqTid) % maxThreads].load();
                if (nodeToHelp == nullptr) continue;
                Node* nodenull = nullptr;
                ltail->next.compare_exchange_strong(nodenull, nodeToHelp);
                break;
            }
            orc_ptr<Node*> lnext = ltail->next.load();
     	    if (lnext != nullptr) tail.compare_exchange_strong(ltail, lnext); // Help a thread do step 3
        }
        enqueuers[tid].store(nullptr, std::memory_order_release); // Do step 4, just in case it's not done
    }


    /**
     * Steps when uncontended:
     * 1. Publish request to dequeue in dequeuers[tid];
     * 2. CAS node->deqTid from IDX_START to tid;
     * 3. Set dequeuers[tid] to the newly owned node;
     * 4. Advance the head with casHead();
     *
     * We must protect either head or tail with HP before doing the check for
     * empty queue, otherwise we may get into retired-deleted-newed-reenqueued.
     *
     * @param tid: The tid must be a UNIQUE index for each thread, in the range 0 to maxThreads-1
     */
    T* dequeue() {
        const int tid = ThreadRegistry::getTID();
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        Node* prReq;
        orc_ptr<Node*> myReq, lhead, lnext;
        prReq = deqself[tid].load();     // Previous request
        myReq = deqhelp[tid].load();
        deqself[tid].store(myReq);             // Step 1
        for (int i=0; i < maxThreads; i++) {
            if (deqhelp[tid].load().ptr != myReq) break;
            lhead = head.load();
            if (lhead == tail.load()) {        // Give up
                deqself[tid].store(prReq);     // Rollback request to dequeue
                giveUp(myReq, tid);
                if (deqhelp[tid].load().ptr != myReq) {
                    deqself[tid].store(myReq, std::memory_order_relaxed);
                    break;
                }
                return nullptr;
            }
            lnext = lhead->next.load();
            if (lhead != head.load().ptr) continue;
 		    if (searchNext(lhead, lnext) != IDX_NONE) casDeqAndHead(lhead, lnext, tid);
        }
        Node* myNode = deqhelp[tid].load().ptr;
        lhead = head.load();     // Do step 4 if needed
        if (lhead == head.load().ptr && myNode == lhead->next.load().ptr) head.compare_exchange_strong(lhead, myNode);
        return myNode->item;
    }
};
