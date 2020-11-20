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

#include "../../trackers/OrcPTP.hpp"

using namespace orcgc_ptp;



/**
 * Harris Linked list modified by Maurice Herlihy and Nir Shavit
 *
 * See "The Art of Multiprocessor programming", section 9.8
 *
 * Memory reclamation is done with OrcGC, which is the only scheme compatible with it
 * 
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free
 * <li>contains(x) - Wait-Free (bounded by key space)
 * </ul><p>
 * <p>
 */
template<typename T>
class HerlihyShavitHarrisLinkedListSetOrcGC {

private:
    struct Node : public orc_base {
        T key;
        orc_atomic<Node*> next {nullptr};

        Node(T key) : key{key} { }
        void poisonAllLinks() { next.poison(); }
    } __attribute__((aligned(128)));

    // Pointers to head and tail sentinel nodes of the list
    orc_atomic<Node*> head;
    orc_atomic<Node*> tail;

public:

    HerlihyShavitHarrisLinkedListSetOrcGC() {
        head = make_orc<Node>(T{});
        tail = make_orc<Node>(T{});
        head->next = tail;
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~HerlihyShavitHarrisLinkedListSetOrcGC() {
        head.store(nullptr);
    }

    static std::string className() { return "HerlihyShavitHarris-LinkedListSet-OrcGC" ; }

    void addAll(T** keys, const int size) {
        for(int i=0;i<size;i++){
            T* key = keys[i];
            add(*key);
        }
    }

    /**
     * 9.25 of the book
     * Progress Condition: Lock-Free
     */
    bool add(T key) {
        orc_ptr<Node*> pred, curr;
        while (true) {
            find(key, pred, curr);
            if (curr != tail && curr->key == key) {
                return false;
            } else {
                orc_ptr<Node*> node = make_orc<Node>(key);
                node->next.store(getUnmarked(curr));
                if (pred->next.compare_exchange_strong(getUnmarked(curr), node)) {
                    return true;
                }
            }
        }
    }


    /**
     * figure 9.26 of the book
     * Lock-Free
     */
    bool remove(T key) {
        orc_ptr<Node*> pred, curr;
        while (true) {
            find(key, pred, curr);
            if (curr == tail) return false;
            if (curr->key != key) {
                return false;
            } else {
                orc_ptr<Node*> succ = curr->next.load();
                if (isMarked(succ)) continue;
                bool snip = curr->next.compare_exchange_strong(succ, getMarked(succ));
                if (!snip) continue;
                pred->next.compare_exchange_strong(getUnmarked(curr), succ);
                return true;
            }
        }
    }


    /**
     * figure 9.27 of the book
     * Progress Condition: Wait-Free (bounded by key space)
     */
    bool contains(T key) {
    	retry:
        orc_ptr<Node*> pred = head.load();
        orc_ptr<Node*> curr = pred->next.load();
        if (curr == tail) return false;
        orc_ptr<Node*> succ = curr->next.load();
        while (curr->key < key) {
        	if(is_poisoned(succ)) goto retry;
            curr.setUnmarked(succ);
            succ = curr->next.load();
            if (curr == tail) return false;
        }
        return (curr->key == key && !isMarked(succ));
    }


private:

    /**
     * Figure 9.24 of the book
     * Progress Condition: Lock-Free
     */
    void find (T key, orc_ptr<Node*>& pred, orc_ptr<Node*>& curr) {
        orc_ptr<Node*> succ;
        retry:
        while (true) {
            pred = head.load();
            curr = pred->next.load();         // pred.next is never marked
            if (curr == tail) return;
            while (true) {
                succ = curr->next.load();
                while (isMarked(succ)) {
                	if(is_poisoned(succ)) goto retry;
                    bool snip = pred->next.compare_exchange_strong(curr, getUnmarked(succ));
                    if (!snip) goto retry;
                    curr->next.poison();
                    curr.setUnmarked(succ);  // equivalent to: curr = getUnamrked(succ)
                    if (curr == tail) return;
                    succ = curr->next.load();
                }
                if (key == curr->key) return;
                if (key < curr->key) return;
                pred = curr;
                curr = succ;
                if (curr == tail) return;
            }
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

