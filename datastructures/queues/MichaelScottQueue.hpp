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
 * <h1> Michael-Scott Queue </h1>
 *
 * enqueue algorithm: MS enqueue
 * dequeue algorithm: MS dequeue
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Templatized
 */
template<typename T, template<typename> class Reclaimer = HazardPointers>
class MichaelScottQueue {

private:
    struct Node : Reclaimer<Node>::BaseObj {
        T* item;
        std::atomic<Node*> next;

        Node(T* userItem) : item{userItem}, next{nullptr} { }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    } __attribute__((aligned(128)));

    bool casTail(Node *cmp, Node *val) {
		return tail.compare_exchange_strong(cmp, val);
	}

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;

    // We need two hazardous pointers for dequeue()
    Reclaimer<Node> hp {2};
    const int kHpTail = 0;
    const int kHpHead = 0;
    const int kHpNext = 1;

public:
    MichaelScottQueue() {
        Node* sentinelNode = new Node(nullptr);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }


    ~MichaelScottQueue() {
        while (dequeue() != nullptr); // Drain the queue
        delete head.load();           // Delete the last node
    }

    static std::string className() { return "MichaelScottQueue-" + Reclaimer<Node>::className(); }

    void enqueue(T* item) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* newNode = new Node(item);
        while (true) {
            Node* ltail = hp.protect(kHpTail, &tail);
            Node* lnext = ltail->next.load();
            if (lnext == nullptr) {
                // It seems this is the last node, so add the newNode here
                // and try to move the tail to the newNode
                if (ltail->casNext(nullptr, newNode)) {
                    casTail(ltail, newNode);
                    hp.clear();
                    return;
                }
            } else {
                casTail(ltail, lnext);
            }
        }
    }


    T* dequeue() {
        Node* node = hp.protect(kHpHead, &head);
        while (node != tail.load()) {
            Node* lnext = hp.protect(kHpNext, &node->next);
            if (casHead(node, lnext)) {
                T* item = lnext->item;  // Another thread may clean up lnext after we do hp.clear()
                hp.clear();
                hp.retire(node);
                return item;
            }
            node = hp.protect(kHpHead, &head);
        }
        hp.clear();
        return nullptr;                  // Queue is empty
    }
};

