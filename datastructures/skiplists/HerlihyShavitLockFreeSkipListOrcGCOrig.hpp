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
#include <limits>
#include <cmath>

#include "../../trackers/OrcPTP.hpp"

using namespace orcgc_ptp;



/**
 * The Lock-free Skiplist in "The Art of Multiprocessor programming", chapter 14
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
class HerlihyShavitLockFreeSkipListOrcGCOrig {

private:

    static const int MAX_LEVEL = 16;

    struct Node : orc_base  {
        T key;
        orc_atomic<Node*> next[MAX_LEVEL+1];
        int topLevel;

        Node(T x) : key{x}{
            for (int i = 0; i <= MAX_LEVEL; i++) next[i] = nullptr;
            topLevel = MAX_LEVEL;
        }

        Node(T x, int hgt): key{x} {
            for (int i = 0; i <= hgt; i++) next[i] = nullptr;
            topLevel = hgt;
        }

        void poisonAllLinks() { for (int i = 0; i <= MAX_LEVEL; i++) next[i].poison(); }
    } __attribute__((aligned(128)));

    // Pointers to head and tail sentinel nodes of the skiplist
    orc_atomic<Node*> head;
    orc_atomic<Node*> tail;

public:

    HerlihyShavitLockFreeSkipListOrcGCOrig() {
    	head = make_orc<Node>(T{});
    	tail = make_orc<Node>(T{});
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head->next[i] = tail;
        }
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~HerlihyShavitLockFreeSkipListOrcGCOrig() {
        head = nullptr;
        tail = nullptr;
    }

    static std::string className() { return "HerlihyShavit-LockFreeSkipListOrcGCOrig" ; }


    float frand() {
        return (float) rand() / RAND_MAX;
    }

    /* Random Level Generator */
    int random_level() {
        static bool first = true;
        if (first){
            srand((unsigned)time(nullptr));
            first = false;
        }

        int lvl = (int)(log(frand()) / log(1.-0.5f));
        return lvl < MAX_LEVEL ? lvl : MAX_LEVEL;
    }

    void addAll(T** keys, const int size) {
        for(int i=0;i<size;i++){
            T* key = keys[i];
            add(*key);
        }
    }

    /**
     * Progress Condition: Lock-Free
     */
    bool add(T key) {
        int topLevel = random_level();
        int bottomLevel = 0;
        orc_ptr<Node*> preds[MAX_LEVEL + 1];
        orc_ptr<Node*> succs[MAX_LEVEL + 1];
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;
            } else {
                orc_ptr<Node*> newNode = make_orc<Node>(key, topLevel);
                for (int level = bottomLevel; level <= topLevel; level++) {
                	orc_ptr<Node*> succ = succs[level];
                	assert(!is_poisoned(succ));
                    newNode->next[level] = succ;
                }
                orc_ptr<Node*> pred = preds[bottomLevel];
                orc_ptr<Node*> succ = succs[bottomLevel];
                newNode->next[bottomLevel] = succ;
                if (!pred->next[bottomLevel].compare_exchange_strong(succ, newNode)) {
                    continue;
                }
                for (int level = bottomLevel+1; level <= topLevel; level++) {
                    while (true) {
                        pred = preds[level];
                        succ = succs[level];
                        if (pred->next[level].compare_exchange_strong(succ, newNode)) break;
                        find(key, preds, succs);
                    }
                }
                return true;
            }
        }
    }


    /**
     * Lock-Free
     */
    bool remove(T key) {
    	const int bottomLevel = 0;
    	orc_ptr<Node*> preds[MAX_LEVEL + 1];
		orc_ptr<Node*> succs[MAX_LEVEL + 1];
		orc_ptr<Node*> succ;
    	while (true) {
    	    bool found = find(key, preds, succs);
    	    if (!found) {
    	        return false;
    	    } else {
    	        orc_ptr<Node*> nodeToRemove = succs[bottomLevel];
    	        for (int level = nodeToRemove->topLevel; level >= bottomLevel+1; level--) {
    	            succ = nodeToRemove->next[level].load();
    	            while (!isMarked(succ)) {
    	                nodeToRemove->next[level].compare_exchange_strong(succ, getMarked(succ));
    	                succ = nodeToRemove->next[level].load();
    	            }
    	        }
    	        succ = nodeToRemove->next[bottomLevel].load();
    	        while (true) {
    	            bool iMarkedIt = nodeToRemove->next[bottomLevel].compare_exchange_strong(getUnmarked(succ), getMarked(succ));
    	            succ = nodeToRemove->next[bottomLevel].load();
    	            if (iMarkedIt) {
    	                find(key, preds, succs);
    	                return true;
    	            }
    	            else if (isMarked(succ)) return false;
    	        }
            }
    	}
    }


    bool contains(T key) {
        int bottomLevel = 0;
        orc_ptr<Node*> pred, curr, succ;
        restart:
        pred = head;
        for (int level = MAX_LEVEL; level >= bottomLevel; level--) {
            curr.setUnmarked(pred->next[level].load());
            while (curr!=tail.load()) {
                succ = curr->next[level].load();
                while (isMarked(succ)) {
                    curr.setUnmarked(succ);
                    succ = curr->next[level].load();
                    if(curr==tail.load()) break;
                }
                if(curr==tail.load()) break;
                if (curr->key < key){
                    pred = curr;
                    curr.setUnmarked(succ);
                } else {
                	break;
                }
            }
        }
        if (curr==tail.load()) return false;
        return (curr->key == key && !isMarked(succ));
    }


private:

    bool find (T key, orc_ptr<Node*>* preds, orc_ptr<Node*>* succs) {
        int bottomLevel = 0;
        bool snip;
        orc_ptr<Node*> pred, curr, succ;
        retry:
		pred = head;
		for (int level = MAX_LEVEL; level >= bottomLevel; level--) {
			curr = pred->next[level].load();
			if(curr!=getUnmarked(curr)) goto retry;
			while (curr != tail) {
				succ = curr->next[level].load();
				while (isMarked(succ)) {
					snip = pred->next[level].compare_exchange_strong(curr, getUnmarked(succ));
					if (!snip) goto retry;
					curr.setUnmarked(pred->next[level].load());
					succ = curr->next[level].load();
					if(curr==tail) break;
				}
				if(curr==tail) break;
				if (curr->key < key){
					pred = curr;
					curr.setUnmarked(succ);
				} else {break;}
			}
			preds[level] = pred;
			succs[level] = curr;
		}
		if (curr==tail.load()) return false;
		return (curr->key == key && !isMarked(succ));

    }

    bool isMarked(Node* node) {
        return ((size_t) node & 0x1ULL);
    }

    Node * getMarked(Node* node) {
        return (Node*)((size_t) node | 0x1ULL);
    }

    Node * getUnmarked(Node* node) {
        return (Node*)((size_t) node & (~0x1ULL));
    }
};

