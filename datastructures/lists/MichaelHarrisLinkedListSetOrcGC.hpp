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
#include <thread>
#include <iostream>
#include <string>

#include "../../trackers/OrcPTP.hpp"

using namespace orcgc_ptp;

/**
 * This is the linked list by Maged M. Michael that uses Hazard Pointers in
 * a correct way because Harris original algorithm with HPs doesn't.
 * Lock-Free Linked List as described in Maged M. Michael paper (Figure 4):
 * http://www.cs.tau.ac.il/~afek/p73-Lock-Free-HashTbls-michael.pdf
 *
 * Original Harris algorithm is different:
 * https://www.microsoft.com/en-us/research/wp-content/uploads/2001/10/2001-disc.pdf
 *
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free
 * <li>contains(x) - Lock-Free
 * </ul><p>
 * <p>
 */
template<typename T>
class MichaelHarrisLinkedListSetOrcGC {

private:
    struct Node : public orc_base {
        T key;
        orc_atomic<Node*> next;

        Node(T key) : key{key}, next{nullptr} { }
        void poisonAllLinks() { next.poison(); }
    } __attribute__((aligned(128)));

    // Pointers to head and tail sentinel nodes of the list
    orc_atomic<Node*> head;
    orc_atomic<Node*> tail;

public:

    MichaelHarrisLinkedListSetOrcGC() {
        head = make_orc<Node>(T{});
        tail = make_orc<Node>(T{});
        head->next = tail;
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~MichaelHarrisLinkedListSetOrcGC() {
        head = nullptr;
    }

    static std::string className() { return "MichaelHarris-LinkedListSet-OrcGC"; }

    void addAll(T** keys, const int size) {
        for(int i=0;i<size;i++){
            T* key = keys[i];
            add(*key);
        }
    }

    /**
     * This method is named 'Insert()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     *
     */
    bool add(T key) {
        orc_ptr<Node*> newNode;
        orc_ptr<Node*> prev, curr, next;
        while (true) {
            if (find(&key, prev, curr, next)) return false;
            if (newNode == nullptr) newNode = make_orc<Node>(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = curr;
            if (prev->next.compare_exchange_strong(tmp, newNode)) return true;
        }
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     */
    bool remove(T key) {
    	orc_ptr<Node*> prev, curr, next;
        while (true) {
            /* Try to find the key in the list. */
            if (!find(&key, prev, curr, next)) return false;
            /* Mark if needed. */
            Node *tmp = next;
            if (!curr->next.compare_exchange_strong(tmp, getMarked(next))) {
                continue; /* Another thread interfered. */
            }
            tmp = curr;
            prev->next.compare_exchange_strong(tmp, next); /* Unlink */
            return true;
        }
    }


    /**
     * This is named 'Search()' on the original paper
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     */
    bool contains(T key) {
        orc_ptr<Node*> prev, curr, next;
        bool ret = find(&key, prev, curr, next);
        return ret;
    }


private:

    /**
     * Progress Condition: Lock-Free
     */
    bool find (T* key, orc_ptr<Node*>& prev, orc_ptr<Node*>& curr, orc_ptr<Node*>& next) {
     try_again:
        prev = head.load();
        curr = prev->next.load();
        while (true) {
        	if (curr == tail) return false;
        	next = curr->next.load();
            Node* un_next = getUnmarked(next);
            if (un_next == next) { // !cmark in the paper
                if (!(curr->key < *key)) { // Check for null to handle head and tail
                    return (curr->key == *key);
                }
                prev = curr;
            } else {
                // Update the link and retire the node.
                Node *tmp = curr;
                if (!prev->next.compare_exchange_strong(tmp, un_next)) {
                	if(prev->next.load()!=un_next) goto try_again;
                }
            }
            curr.setUnmarked(next);
        }
    }


    bool isMarked(Node * node) {
    	return ((size_t) node & 0x1ULL);
    }

    Node * getMarked(Node * node) {
    	return (Node*)((size_t) node | 0x1ULL);
    }

    Node * getUnmarked(Node * node) {
    	return (Node*)((size_t) node & (~0x1ULL));
    }
};

