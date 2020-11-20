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
 * <h1> Michael-Scott Queue </h1>
 *
 * enqueue algorithm: MS enqueue
 * dequeue algorithm: MS dequeue
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: OrcGC-HE
 */
template<typename T>
class MichaelScottQueueOrcGC {

private:
    struct Node : orc_base {
        T* item;
        orc_atomic<Node*> next {nullptr};

        Node(T* userItem) : item{userItem} { }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
        void poisonAllLinks() { next.poison(); }
    } __attribute__((aligned(128)));

    // Pointers to head and tail of the list
    alignas(128) orc_atomic<Node*> head;
    alignas(128) orc_atomic<Node*> tail;


public:
    MichaelScottQueueOrcGC() {
        auto sentinelNode = make_orc<Node>(nullptr);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }


    ~MichaelScottQueueOrcGC() {
        while (dequeue() != nullptr); // Drain the queue
    }


    static std::string className() { return "MichaelScottQueue-OrcGC"; }


    void enqueue(T* item) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        orc_ptr<Node*> newNode = make_orc<Node>(item);
        while (true) {
            orc_ptr<Node*> ltail = tail.load();              // orc_ptr
            orc_ptr<Node*> lnext = ltail->next.load();       // orc_ptr
            if (lnext == nullptr) {
                // It seems this is the last node, so add the newNode here
                // and try to move the tail to the newNode
                if (ltail->next.compare_exchange_strong(nullptr, newNode)) {
                	tail.compare_exchange_strong(ltail, newNode);
                    return;
                }
            } else {
            	tail.compare_exchange_strong(ltail, lnext);
            }
        }
    }


    T* dequeue() {
        orc_ptr<Node*> node = head.load();                              // orc_ptr
        while (node != tail.load().ptr) {
            orc_ptr<Node*> lnext = node->next.load();                   // orc_ptr
            if (head.compare_exchange_strong(node, lnext)) {
            	node->next.poison();
            	return lnext->item;
            }
            node = head.load();
        }
        return nullptr;                  // Queue is empty
    }
};

