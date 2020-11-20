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
#include "trackers/HazardPointers.hpp"
#include "trackers/PassThePointer.hpp"


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
template<typename T, template<typename> class Reclaimer = HazardPointers>
class TreiberStack {

private:
    struct Node : Reclaimer<Node>::BaseObj {
        T* item;
        Node* next;

        Node(T* item, Node* lhead) : item{item}, next{lhead} {}
    } __attribute__((aligned(128)));

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    alignas(128) std::atomic<Node*> head;

    // We need one hazardous pointer for pop()
    Reclaimer<Node> he {1};
    const int kHpHead = 0;

    Node* sentinel = new Node(nullptr, nullptr);


public:
    TreiberStack() {
        head.store(sentinel, std::memory_order_relaxed);
    }


    ~TreiberStack() {
        while (pop() != nullptr); // Drain the stack
        delete head.load();        // Delete the last node
    }


    static std::string className() { return "TreiberStack-" + Reclaimer<Node>::className(); }


    bool push(T* item) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* lhead = head.load();
        Node* newNode = new Node(item, lhead);
        while (!head.compare_exchange_weak(lhead, newNode)) { // lhead gets updated in case of failure
            newNode->next = lhead;
        }
        return true;
    }


    T* pop() {
        T* item = nullptr;
        while (true) {
            Node* lhead = he.protect(kHpHead, &head);
            if (lhead == sentinel) break; // stack is empty
            if (head.compare_exchange_weak(lhead, lhead->next)) {
                item = lhead->item;
                // No need to nullify before
                he.retire(lhead);
                break;
            }
        }
        he.clear();
        return item;
    }
};
