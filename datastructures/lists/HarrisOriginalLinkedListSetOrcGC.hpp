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
 * Original Harris Linked list.
 *
 * https://www.microsoft.com/en-us/research/wp-content/uploads/2001/10/2001-disc.pdf
 * This is unsuitable to be used with Hazard Pointers as explained by Cohen in
 * "Every data structure deseres lock-free reclamation":
 * https://dl.acm.org/doi/10.1145/3276513
 *
 * Memory reclamation is done with OrcGC, which is the only scheme compatible with it
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
class HarrisOriginalLinkedListSetOrcGC {

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

    HarrisOriginalLinkedListSetOrcGC() {
        head = make_orc<Node>(T{});
        tail = make_orc<Node>(T{});
        head->next = tail;
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~HarrisOriginalLinkedListSetOrcGC() {
        head.store(nullptr);
    }

    static std::string className() { return "HarrisOriginal-LinkedListSet-OrcGC" ; }

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
        orc_ptr<Node*> new_node = make_orc<Node>(key);
        orc_ptr<Node*> left_node;
        do {
            orc_ptr<Node*> right_node = search (key, left_node);
            if ((right_node != tail) && (right_node->key == key)) /*T1*/
                return false;
            new_node->next.store(right_node);
            if (left_node->next.compare_exchange_strong(right_node, new_node)) /*C2*/
                return true;
        } while (true); /*B3*/
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     */
    bool remove(T key) {
        orc_ptr<Node*> right_node;
        orc_ptr<Node*> right_node_next;
        orc_ptr<Node*> left_node;
        do {
            right_node = search (key, left_node);
            if ((right_node == tail) || (right_node->key != key)) /*T1*/
                return false;
            right_node_next = right_node->next.load();
            if (!isMarked(right_node_next))
                if (right_node->next.compare_exchange_strong(right_node_next, getMarked(right_node_next))) break;
        } while (true); /*B4*/
        if (!left_node->next.compare_exchange_strong(right_node, right_node_next)) /*C4*/
            right_node = search (right_node->key, left_node);
        return true;
    }


    /**
     * This is named 'Search()' on the original paper
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     */
    bool contains(T key) {
        orc_ptr<Node*> right_node;
        orc_ptr<Node*> left_node;
        right_node = search (key, left_node);
        if ((right_node == tail) || (right_node->key != key))
            return false;
        else
            return true;
    }


private:

    /**
     * Progress Condition: Lock-Free
     */
    orc_ptr<Node*> search (T search_key, orc_ptr<Node*>& left_node) {
        search_again:
        do {
            orc_ptr<Node*> left_node_next;
            orc_ptr<Node*> right_node = head.load();
            orc_ptr<Node*> t_next = right_node->next.load(); /* 1: Find left_node and right_node */
            do {
                if (!isMarked(t_next)) {
                    left_node = right_node;
                    left_node_next = t_next;
                }
                right_node.setUnmarked(t_next);
                if (right_node == tail) break;
                t_next = right_node->next.load();
            } while (isMarked(t_next) || (right_node->key < search_key)); /*B1*/
            /* 2: Check nodes are adjacent */
            if (left_node_next == right_node)
                if ((right_node != tail) && isMarked(orc_ptr<Node*>{right_node->next.load()})) goto search_again; /*G1*/
            else
                return right_node; /*R1*/
            /* 3: Remove one or more marked nodes */
            if (left_node->next.compare_exchange_strong(left_node_next, right_node)) /*C1*/
                if ((right_node != tail) && isMarked(orc_ptr<Node*>{right_node->next.load()})) goto search_again; /*G2*/
            else
                return right_node; /*R2*/
        } while (true); /*B2*/
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

