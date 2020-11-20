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
#include <forward_list>
#include <set>
#include <iostream>
#include <string>
#include "trackers/HazardPointers.hpp"


/**
 * This is the linked list by Maged M. Michael that uses Hazard Pointers in
 * a correct way because Harris original algorithm with HPs doesn't.
 * Lock-Free Linked List as described in Maged M. Michael paper (Figure 4):
 * http://www.cs.tau.ac.il/~afek/p73-Lock-Free-HashTbls-michael.pdf
 *
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
template<typename T, template<typename> class Reclaimer = HazardPointers>
class MichaelHarrisLinkedListSet {

private:
    struct Node : Reclaimer<Node>::BaseObj {
        T key;
        std::atomic<Node*> next;

        Node(T key) : key{key}, next{nullptr} { }
    } __attribute__((aligned(128)));

    // Pointers to head and tail sentinel nodes of the list
    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    // We need 3 hazard pointers
    Reclaimer<Node> hp {3};
    const int kHp0 = 0; // Protects next
    const int kHp1 = 1; // Protects curr
    const int kHp2 = 2; // Protects prev

public:

    MichaelHarrisLinkedListSet() {
        head.store(new Node({}));
        tail.store(new Node({}));
        head.load()->next.store(tail.load());
    }


    // We MichaelHarrisLinkedListSet expect the destructor to be called if this instance can still be in use
    ~MichaelHarrisLinkedListSet() {
        Node *prev = head.load();
        Node *node = prev->next.load();
        while (node != nullptr) {
            delete prev;
            prev = node;
            node = prev->next.load();
        }
        delete prev;
    }

    static std::string className() { return "MichaelHarris-LinkedListSet-" + Reclaimer<Node>::className(); }

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
        Node *curr, *next;
        Node *prev;
        Node* newNode = nullptr;
        while (true) {
            if (find(&key, prev, curr, next)) {
                if (newNode != nullptr) delete newNode;              // There is already a matching key
                hp.clear();
                return false;
            }
            if (newNode == nullptr) newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = curr;
            if (prev->next.compare_exchange_strong(tmp, newNode)) { // seq-cst
                hp.clear();
                return true;
            }
        }
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     */
    bool remove(T key) {
        Node *curr, *next;
        Node *prev;
        while (true) {
            /* Try to find the key in the list. */
            if (!find(&key, prev, curr, next)) {
                hp.clear();
                return false;
            }
            /* Mark if needed. */
            Node *tmp = next;
            if (!curr->next.compare_exchange_strong(tmp, getMarked(next))) {
                continue; /* Another thread interfered. */
            }

            tmp = curr;
            if (prev->next.compare_exchange_strong(tmp, next)) { /* Unlink */
                hp.clear();
                hp.retire(curr); /* Reclaim */
            } else {
                hp.clear();
            }
            /*
             * If we want to prevent the possibility of there being an
             * unbounded number of marked nodes, add "else _find(head,key)."
             * This is not necessary for correctness.
             */
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
        Node *curr, *next;
        Node *prev;
        bool isContains = find(&key, prev, curr, next);
        hp.clear();
        return isContains;
    }


private:

    /**
     * TODO: This needs to be code reviewed... it's not production-ready
     * <p>
     * Progress Condition: Lock-Free
     */
    bool find (T* key, Node*& prev, Node*& curr, Node*& next) {

     try_again:
        prev = head.load();
        curr = prev->next.load();
        // Protect curr with a hazard pointer.
        hp.protectPtr(kHp1, curr);
        if (prev->next.load() != curr) goto try_again;
        while (true) {
        	if (curr == tail.load()) return false;
            // Protect next with a hazard pointer.
        	publish:
            next = curr->next.load();
            Node* un_next = getUnmarked(next);
            hp.protectPtr(kHp0, un_next);
            if(curr->next.load()!=next) goto publish;
            if (un_next == next) { // !cmark in the paper
                if (!(curr->key < *key)) { // Check for null to handle head and tail
                    return (curr->key == *key);
                }
                prev = curr;
                hp.protectPtrRelease(kHp2, curr, kHp1);
            } else {
                // Update the link and retire the node.
                Node *tmp = curr;
                if (!prev->next.compare_exchange_strong(tmp, un_next)) {
                	if (prev->next.load() != un_next) goto try_again;
                } else {
                	hp.retire(curr);
                }
            }
            curr = un_next;
            hp.protectPtrRelease(kHp1, curr, kHp0);
        }
    }

    bool isMarked(Node * node) {
    	return ((size_t) node & 0x1);
    }

    Node * getMarked(Node * node) {
    	return (Node*)((size_t) node | 0x1);
    }

    Node * getUnmarked(Node * node) {
    	return (Node*)((size_t) node & (~0x1));
    }
};

