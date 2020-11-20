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
 * Wait-Free Linked List
 *
 * Taken from the paper "Wait-Free Linked Lists" (Appendix B) by
 * Shahar Timnat, Anastasia Braginsky, Alex Kogan, Erez Petrank
 * http://www.cs.technion.ac.il/~erez/Papers/wfll-full.pdf
 * http://www.cs.technion.ac.il/~erez/Papers/wfll.pdf
 *
 * We don't use a "Window", we just pass the pred and curr by reference to the helper functions.
 *
 * Memory reclamation is done with OrcGC
 * 
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Wait-Free
 * <li>remove(x)   - Wait-Free
 * <li>contains(x) - Wait-Free
 * </ul><p>
 * <p>
 */
template<typename T>
class TBKPLinkedListSetOrcGC {

private:

    enum OpType {insertOp, search_delete, execute_delete, success, failure, determine_delete, containsOp };

    // forward declaration of Node
    struct Node;

    // L229-L236, Appendix B
    struct ReferenceBooleanTriplet : public orc_base {
        orc_atomic<Node*> reference;
        const bool        bit;
        const uint64_t    version;
        ReferenceBooleanTriplet(Node* r, bool i, uint64_t v) : reference{r}, bit{i}, version{v} {}
        void poisonAllLinks() { reference.poison(); }
    } __attribute__((aligned(128)));

    // L240-L242
    struct VersionedAtomicMarkableReference : public orc_base {
        orc_atomic<ReferenceBooleanTriplet*> atomicRef;

        // Extra constructor to initialize at {nullptr,false}
        VersionedAtomicMarkableReference(Node* initialRef, bool initialMark) {
            atomicRef.store(make_orc<ReferenceBooleanTriplet>(initialRef, initialMark, 0)); // TODO: replace with operator=
        }

        // L244-L246
        orc_ptr<Node*> getReference() {
            orc_ptr<ReferenceBooleanTriplet*> aref = atomicRef.load();
            return std::move(aref->reference.load());
        }

        // L248-L250
        bool isMarked() {
            orc_ptr<ReferenceBooleanTriplet*> aref = atomicRef.load();
            return aref->bit;
        }

        // L252-256
        orc_ptr<Node*> get(bool& markHolder) {
            orc_ptr<ReferenceBooleanTriplet*> current = atomicRef.load();
            markHolder = current->bit;
            return std::move(current->reference.load());
        }

        // L271-L282
        bool compareAndSet(Node* expectedReference, Node* newReference, bool expectedMark, bool newMark) {
            orc_ptr<ReferenceBooleanTriplet*> current = atomicRef.load();
            orc_ptr<ReferenceBooleanTriplet*> newrbt = make_orc<ReferenceBooleanTriplet>(newReference, newMark, current->version+1);
            return expectedReference == current->reference &&
                    expectedMark == current->bit &&
                    ((newReference == current->reference && newMark == current->bit) ||
                            atomicRef.compare_exchange_strong(current, newrbt));
        }

        // L284-288
        void set(Node* newReference, bool newMark) {
            orc_ptr<ReferenceBooleanTriplet*> current = atomicRef.load();
            if (newReference != current->reference || newMark != current->bit)
                atomicRef.store(make_orc<ReferenceBooleanTriplet>(newReference, newMark, current->version+1));
        }

        // L290-L297
        bool attemptMark(Node* expectedReference, bool newMark) {
            orc_ptr<ReferenceBooleanTriplet*> current = atomicRef.load();
            orc_ptr<ReferenceBooleanTriplet*> newrbt = make_orc<ReferenceBooleanTriplet>(expectedReference, newMark, current->version+1);
            return expectedReference == current->reference &&
                    (newMark == current->bit || atomicRef.compare_exchange_strong(current, newrbt));
        }

        // L299-L302
        uint64_t getVersion() {
            return atomicRef->version;
        }

        // L304-L312
        bool compareAndSet(uint64_t version, Node* expectedReference, Node* newReference, bool expectedMark, bool newMark) {
            orc_ptr<ReferenceBooleanTriplet*> current = atomicRef.load();
            orc_ptr<ReferenceBooleanTriplet*> newrbt = make_orc<ReferenceBooleanTriplet>(newReference, newMark, current->version+1);
            return expectedReference == current->reference &&
                    expectedMark == current->bit && version == current->version &&
                    ((newReference == current->reference && newMark == current->bit) ||
                            atomicRef.compare_exchange_strong(current, newrbt));
        }

        void poisonAllLinks() { atomicRef.poison(); }
    };

    struct Node : public orc_base {
        T key;
        VersionedAtomicMarkableReference next{nullptr, false};
        std::atomic<bool> d {false};

        Node(T key) : key{key} { }
        void poisonAllLinks() { next.poisonAllLinks(); }
    } __attribute__((aligned(128)));

    struct Window : public orc_base {
        orc_atomic<Node*> pred;
        orc_atomic<Node*> curr;
        Window(orc_ptr<Node*>& p, orc_ptr<Node*>& c) : pred{p}, curr{c} {}
        void poisonAllLinks() { pred.poison(); curr.poison(); }
    } __attribute__((aligned(128)));

    struct OpDesc : public orc_base {
        uint64_t            phase;
        OpType              type;
        orc_atomic<Node*>   node;
        orc_atomic<Window*> searchResult;
        OpDesc(uint64_t ph, OpType ty, Node* n, Window* sResult) : phase{ph}, type{ty}, node{n}, searchResult{sResult} {}
        void poisonAllLinks() { node.poison(); searchResult.poison(); }
    } __attribute__((aligned(128)));

    // Pointers to head and tail sentinel nodes of the list
    alignas(128) orc_atomic<Node*>     head;
    alignas(128) orc_atomic<Node*>     tail;
    alignas(128) orc_atomic<OpDesc*>   state[REGISTRY_MAX_THREADS];
    alignas(128) std::atomic<uint64_t> currentMaxPhase;

public:

    TBKPLinkedListSetOrcGC() {
        currentMaxPhase = 0;
        head = make_orc<Node>(T{});
        tail = make_orc<Node>(T{});
        orc_ptr<Node*> lhead = head.load();
        lhead->next.set(tail, false);
        orc_ptr<OpDesc*> OPDESC_END = make_orc<OpDesc>(0, OpType::success, nullptr, nullptr);
        for (int i = 0; i < REGISTRY_MAX_THREADS; i++) state[i] = OPDESC_END;
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~TBKPLinkedListSetOrcGC() {
        head = nullptr;
    }

    static std::string className() { return "TBKP-LinkedListSet-OrcGC" ; }

    void addAll(T** keys, const int size) {
        for(int i=0;i<size;i++){
            T* key = keys[i];
            add(*key);
        }
    }

    /**
     * L45-52, Appendix B, page 45
     * Progress Condition: Wait-Free
     */
    bool add(T key) {
        const int tid = ThreadRegistry::getTID();
        uint64_t phase = maxPhase();
        orc_ptr<Node*> newNode = make_orc<Node>(key);
        orc_ptr<OpDesc*> op = make_orc<OpDesc>(phase, OpType::insertOp, newNode, nullptr);
        state[tid].store(op);
        help(phase);
        op = state[tid].load();
        return (op->type == OpType::success);
    }

    /**
     * L54-L63, Appendix B, page 45
     * Progress condition: Wait-Free
     */
    bool remove(T key) {
        const int tid = ThreadRegistry::getTID();
        uint64_t phase = maxPhase();
        orc_ptr<OpDesc*> op = make_orc<OpDesc>(phase, OpType::search_delete, make_orc<Node>(key), nullptr);
        state[tid].store(op);
        help(phase);
        op = state[tid].load();
        if (op->type == OpType::determine_delete) {
            orc_ptr<Node*> curr = op->searchResult->curr.load();
            bool expected = false;
            return curr->d.compare_exchange_strong(expected, true);
        }
        return false;
    }

    /**
     * L185-L192, Appendix B, page 48
     * Progress Condition: Wait-Free
     */
    bool contains(T key) {
        const int tid = ThreadRegistry::getTID();
        uint64_t phase = maxPhase();
        orc_ptr<Node*> newNode = make_orc<Node>(key);
        orc_ptr<OpDesc*> op = make_orc<OpDesc>(phase, OpType::containsOp, newNode, nullptr);
        state[tid].store(op);
        help(phase);
        return (state[tid]->type == OpType::success);
    }


private:

    // L65-L87, Appendix B
    bool search(T key, orc_ptr<Node*>& pred, orc_ptr<Node*>& curr, const int tid, uint64_t phase) {
        orc_ptr<Node*> succ;
        retry:
        while (true) {
            pred = head.load();
            curr = pred->next.getReference();
            if (curr == tail) return true;
            while (true) {
                bool marked;
                succ = curr->next.get(marked);
                while (marked) {
                    bool snip = pred->next.compareAndSet(curr, succ, false, false);
                    if (!isSearchStillPending(tid, phase)) return false;
                    if (!snip) goto retry;
                    curr = succ;
                    if (curr == tail) return true;
                    succ = curr->next.get(marked);
                }
                if (key == curr->key) return true;
                if (key < curr->key) return true;
                pred = curr;
                curr = succ;
                if (curr == tail) return true;
            }
        }
    }

    // L89-L101, Appendix B
    void help(uint64_t phase) {
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        for (int i = 0; i < maxThreads; i++) {
            orc_ptr<OpDesc*> desc = state[i].load();
            if (desc->phase <= phase) {
                if (desc->type == OpType::insertOp) {
                    helpInsert(i, desc->phase);
                } else if (desc->type == OpType::search_delete || desc->type == OpType::execute_delete) {
                    helpRemove(i, desc->phase);
                } else if (desc->type == OpType::containsOp) {
                    helpContains(i, desc->phase);
                }
            }
        }
    }

    // L103-L147, Appendix B
    void helpInsert(const int tid, uint64_t phase) {
        orc_ptr<Node*> node_next, pred, curr;
        while (true) {
            orc_ptr<OpDesc*> op = state[tid].load();
            if (!(op->type == OpType::insertOp && op->phase == phase)) return;
            orc_ptr<Node*> node = op->node.load();
            node_next = node->next.getReference();
            if (!search(node->key, pred, curr, tid, phase)) return;
            if (curr != tail && curr->key == node->key) {
                if (curr == node || node->next.isMarked()) {
                    orc_ptr<OpDesc*> successOp = make_orc<OpDesc>(phase, OpType::success, node, nullptr);
                    if (state[tid].compare_exchange_strong(op, successOp)) return;
                } else {
                    orc_ptr<OpDesc*> failOp = make_orc<OpDesc>(phase, OpType::failure, node, nullptr);
                    if (state[tid].compare_exchange_strong(op, failOp)) return;
                }
            } else {
                if (node->next.isMarked()) {
                    orc_ptr<OpDesc*> successOp = make_orc<OpDesc>(phase, OpType::success, node, nullptr);
                    if (state[tid].compare_exchange_strong(op, successOp)) return;
                }
                uint64_t version = pred->next.getVersion();
                orc_ptr<OpDesc*> newOp = make_orc<OpDesc>(phase, OpType::insertOp, node, nullptr);
                if (!state[tid].compare_exchange_strong(op, newOp)) continue;
                node->next.compareAndSet(node_next, curr, false, false);
                if (pred->next.compareAndSet(version, node->next.getReference(), node, false, false)) {
                    orc_ptr<OpDesc*> successOp = make_orc<OpDesc>(phase, OpType::success, node, nullptr);
                    if (state[tid].compare_exchange_strong(op, successOp)) return;
                }
            }
        }
    }

    // L149-L183, Appendix B
    void helpRemove(const int tid, uint64_t phase) {
        orc_ptr<Node*> node_next, pred, curr;
        while (true) {
            orc_ptr<OpDesc*> op = state[tid].load();
            if (!((op->type == OpType::search_delete || op->type == OpType::execute_delete) && op->phase == phase)) return;
            orc_ptr<Node*> node = op->node.load();
            if (op->type == OpType::search_delete) {
                if (!search(node->key, pred, curr, tid, phase)) continue;
                if (curr->key != node->key) {
                    orc_ptr<OpDesc*> failOp = make_orc<OpDesc>(phase, OpType::failure, node, nullptr);
                    if (state[tid].compare_exchange_strong(op, failOp)) return;
                } else {
                    orc_ptr<Window*> window = make_orc<Window>(pred, curr);
                    orc_ptr<OpDesc*> foundOp = make_orc<OpDesc>(phase, OpType::execute_delete, node, window);
                    state[tid].compare_exchange_strong(op, foundOp);
                }
            } else if (op->type == OpType::execute_delete) {
                orc_ptr<Window*> searchResult = op->searchResult.load();
                curr = searchResult->curr.load();
                orc_ptr<Node*> next = curr->next.getReference();
                if (!curr->next.attemptMark(next, true)) continue;
                orc_ptr<Node*> node = op->node.load();
                search(node->key, pred, curr, tid, phase);
                orc_ptr<OpDesc*> determineOp = make_orc<OpDesc>(op->phase, OpType::determine_delete, node, searchResult);
                state[tid].compare_exchange_strong(op, determineOp);
                return;
            }
        }
    }

    // L194-L210, Appendix B
    void helpContains(const int tid, uint64_t phase) {
        orc_ptr<Node*> pred, curr;
        orc_ptr<OpDesc*> op = state[tid].load();
        if (!((op->type == OpType::containsOp) && op->phase == phase)) return;
        orc_ptr<Node*> node = op->node.load();
        if (!search(node->key, pred, curr, tid, phase)) return;
        if (curr != tail && curr->key == node->key) {
            orc_ptr<OpDesc*> successOp = make_orc<OpDesc>(phase, OpType::success, node, nullptr);
            state[tid].compare_exchange_strong(op, successOp);
        } else {
            orc_ptr<OpDesc*> failOp = make_orc<OpDesc>(phase, OpType::failure, node, nullptr);
            state[tid].compare_exchange_strong(op, failOp);
        }
    }

    // L212-L217, Appendix B
    uint64_t maxPhase() {
        uint64_t result = currentMaxPhase.load();
        uint64_t tmp = result;
        currentMaxPhase.compare_exchange_strong(tmp, result+1);
        return result;
    }

    // L219-L225, Appendix B
    bool isSearchStillPending(const int tid, uint64_t ph) {
        orc_ptr<OpDesc*> curr = state[tid].load();
        return (curr->type == OpType::insertOp || curr->type == OpType::search_delete ||
                curr->type == OpType::execute_delete || curr->type == OpType::containsOp) &&
                curr->phase == ph;
    }
};

