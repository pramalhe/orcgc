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
 * <h1> R. Kent Treiber's Stack </h1>
 *
 * The lock-free stack designed by R. K Treiber
 *
 * push algorithm: Treiber
 * pop algorithm: Treiber
 * Consistency: Linearizable
 * push() progress: lock-free
 * pop() progress: lock-free
 * Memory unbounded: singly-linked list based
 * Memory Reclamation: Hazard Pointers (lock-free), only for pop()
 *
 * Link to paper:
 * "Systems programming: Coping with parallelism. Technical Report RJ 511"
 * http://domino.research.ibm.com/library/cyberdig.nsf/papers/58319A2ED2B1078985257003004617EF/$File/rj5118.pdf
 *
 */
template<typename T>
class TreiberStackOrcGC {
private:
    struct Node : public orc_base {
        T* item;
        orc_atomic<Node*> next {nullptr};
        Node(T* item, Node* lnext) : item{item}, next{lnext} { }
    } __attribute__((aligned(128)));

    alignas(128) orc_atomic<Node*> head;

    Node* sentinel;

public:
    TreiberStackOrcGC() {
        head = make_orc<Node>(nullptr, nullptr);
    }


    ~TreiberStackOrcGC() {
        while (pop() != nullptr); // Drain the stack
        //delete sentinel;
    }


    static std::string className() { return "TreiberStack-OrcGC"; }


    bool push(T* item) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        orc_ptr<Node*> lhead = head.load();
        orc_ptr<Node*> newNode = make_orc<Node>(item, lhead);
        while (true) {
            if (head.compare_exchange_weak(lhead, newNode)) return true;
            lhead = head.load();
            newNode->next.store(lhead, std::memory_order_relaxed);
        }
    }


    T* pop() {
        while (true) {
            orc_ptr<Node*> lhead = head.load();
            if (lhead == sentinel) return nullptr; // stack is empty
            orc_ptr<Node*> lnext = lhead->next.load();
            if (head.compare_exchange_weak(lhead, lnext)) {
                lhead->next.poison();
                return lhead->item;
            }
        }
    }
};
