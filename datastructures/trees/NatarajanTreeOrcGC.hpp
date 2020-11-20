/*
 * Copyright 2020
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 *
 * Adapted from https://github.com/roghnin/Interval-Based-Reclamation/blob/master/src/rideables/NatarajanTree.hpp
 *
 * Due to the usage of <optional>, this needs C++17 to compile
 *
 * This needs to be optimized
 */

#pragma once

#include <iostream>
#include <atomic>
#include <algorithm>
#include <map>
#include <optional>

#include "../../trackers/OrcPTP.hpp"

using namespace orcgc_ptp;


template <class K, class V>
class NatarajanTreeOrcGC {
private:

    /* structs*/
    struct Node : orc_base {
        int level;
        K key;
        V val;
        orc_atomic<Node*> left;
        orc_atomic<Node*> right;

        Node(K k, V v, Node* l, Node* r,int lev):level(lev),key(k),val(v),left(l),right(r) {};
        Node(K k, V v, Node* l, Node* r):level(-1),key(k),val(v),left(l),right(r) {};
    } __attribute__((aligned(128)));

    struct SeekRecord {
        orc_ptr<Node*> ancestor;
        orc_ptr<Node*> successor;
        orc_ptr<Node*> parent;
        orc_ptr<Node*> leaf;

        void clear() {
            ancestor = nullptr;
            successor = nullptr;
            parent = nullptr;
            leaf = nullptr;
        }
    } __attribute__((aligned(128)));

    K infK{};
    V defltV{};
    orc_atomic<Node*> r {nullptr};
    orc_atomic<Node*> s {nullptr};

    /* helper functions */
    //flag and tags helpers
    inline Node* mixPtrFlgTg(Node* ptr, bool flg, bool tg){
        Node* tmp =  (Node*) ((size_t)ptr | flg | ((size_t)tg<<1));
        //printf("mixPtrFlgTg() = %p\n", tmp);
        return tmp;
    }
    //node comparison
    inline bool isInf(orc_ptr<Node*>& n){
        return getInfLevel(n)!=-1;
    }
    inline int getInfLevel(orc_ptr<Node*>& n){
        // 0 for inf0, 1 for inf1, 2 for inf2, -1 for general val
        return n->level;
    }
    inline bool nodeLess(orc_ptr<Node*>& n1, orc_ptr<Node*>& n2) {
        int i1=getInfLevel(n1);
        int i2=getInfLevel(n2);
        return i1<i2 || (i1==-1&&i2==-1&&n1->key<n2->key);
    }
    // Used only by seek()
    inline bool keyLess(K key, orc_ptr<Node*>& n2) {
        return (key < n2->key);
    }
    inline bool nodeEqual(orc_ptr<Node*>& n1, orc_ptr<Node*>& n2){
        int i1=getInfLevel(n1);
        int i2=getInfLevel(n2);
        if(i1==-1&&i2==-1)
            return n1->key==n2->key;
        else
            return i1==i2;
    }
    // Used only by seek()
    inline bool keyEqual(K key, orc_ptr<Node*>& n2){
        return key == n2->key;
    }
    inline bool nodeLessEqual(orc_ptr<Node*>& n1, orc_ptr<Node*>& n2){
        return !nodeLess(n2,n1);
    }

    /* private interfaces */
    void seek(K key, SeekRecord& seekRecord);
    bool cleanup(K key, SeekRecord& seekRecord);
    void doRangeQuery(Node& k1, Node& k2, orc_ptr<Node*>& root, std::map<K,V>& res);
public:
    NatarajanTreeOrcGC() {
        r = make_orc<Node>(infK,defltV,nullptr,nullptr,2);
        s = make_orc<Node>(infK,defltV,nullptr,nullptr,1);
        r->right = make_orc<Node>(infK,defltV,nullptr,nullptr,2);
        r->left = s;
        s->right = make_orc<Node>(infK,defltV,nullptr,nullptr,1);
        s->left = make_orc<Node>(infK,defltV,nullptr,nullptr,0);
        // No need to use make_orc for 'records' because it's a static array
        //records = new SeekRecord[REGISTRY_MAX_THREADS]{};
        //for (int i = 0; i < REGISTRY_MAX_THREADS; i++) records[i].clear();
    };

    ~NatarajanTreeOrcGC() {
        r = nullptr;
        s = nullptr;
    };

    static std::string className() { return "NatarajanTree-OrcGC"; }
    std::optional<V> get(K key);
    std::optional<V> put(K key, V val);
    bool insert(K key, V val);
    std::optional<V> innerRemove(K key);
    std::optional<V> replace(K key, V val);
    std::map<K, V> rangeQuery(K key1, K key2, int& len);

    // Used only by our tree benchmarks
    bool add(K key);
    bool remove(K key);
    bool contains(K key);
    void addAll(K** keys, const int size);
};

//-------Definition----------
template <class K, class V>
void NatarajanTreeOrcGC<K,V>::seek(K key, SeekRecord& seekRecord){
    /* initialize the seek record using sentinel nodes */
    seekRecord.ancestor = r;
    seekRecord.successor = r->left;
    seekRecord.parent = seekRecord.successor;
    seekRecord.leaf = s->left;  // We took out getPtr() here because the orc_ptr deals with it

    /* initialize other variables used in the traversal */
    orc_ptr<Node*> parentField = seekRecord.parent->left;
    orc_ptr<Node*> currentField = seekRecord.leaf->left;
    orc_ptr<Node*> current = currentField; // we took out getPtr()

    /* traverse the tree */
    while (current.getUnmarked() != nullptr) {
        /* check if the edge from the current parent node is tagged */
        if (!parentField.getTag()){
            /*
             * found an untagged edge in the access path;
             * advance ancestor and successor pointers.
             */
            //seekRecord.ancestor = seekRecord.parent;
            seekRecord.ancestor.swapPtrs(seekRecord.parent);
            seekRecord.successor = seekRecord.leaf;
            //seekRecord.successor.swapPtrs(seekRecord.leaf);
        }

        /* advance parent and leaf pointers */
        //seekRecord.parent = seekRecord.leaf;
        seekRecord.parent.swapPtrs(seekRecord.leaf);
        seekRecord.leaf = current;
        //seekRecord.leaf.swapPtrs(current);

        /* update other variables used in traversal */
        parentField = currentField;
        if (keyLess(key,current)) {
            currentField = current->left;
        } else {
            currentField = current->right;
        }
        current = currentField;
    }
    /* traversal complete */
    return;
}

template <class K, class V>
bool NatarajanTreeOrcGC<K,V>::cleanup(K key, SeekRecord& seekRecord) {
    orc_ptr<Node*> keyNode = make_orc<Node>(key,defltV,nullptr,nullptr);//node to be compared
    bool res=false;

    orc_ptr<Node*> ancestor = seekRecord.ancestor;
    orc_ptr<Node*> successor = seekRecord.successor;
    orc_ptr<Node*> parent = seekRecord.parent;
    orc_ptr<Node*> leaf = seekRecord.leaf;
    ancestor.unmark();
    successor.unmark();
    parent.unmark();
    leaf.unmark();

    orc_atomic<Node*>* successorAddr=nullptr;
    orc_atomic<Node*>* childAddr=nullptr;
    orc_atomic<Node*>* siblingAddr=nullptr;

    /* obtain address of field of ancestor node that will be modified */
    if (nodeLess(keyNode,ancestor)) {
        successorAddr = &(ancestor->left);
    } else {
        successorAddr = &(ancestor->right);
    }

    /* obtain addresses of child fields of parent node */
    if (nodeLess(keyNode,parent)) {
        childAddr = &(parent->left);
        siblingAddr = &(parent->right);
    } else {
        childAddr = &(parent->right);
        siblingAddr = &(parent->left);
    }
    orc_ptr<Node*> tmpChild = childAddr->load();
    if (!tmpChild.getFlag()) {
        /* the leaf is not flagged, thus sibling node should be flagged */
        tmpChild = siblingAddr->load();
        /* switch the sibling address */
        siblingAddr = childAddr;
    }

    /* use TAS to tag sibling edge */
    while(true){
        orc_ptr<Node*> untagged = siblingAddr->load();
        // No need to make 'tagged' an orc_ptr: 'untagged' will protect the object
        Node* tagged = mixPtrFlgTg(untagged.getUnmarked(),untagged.getFlag(),true);
        if (siblingAddr->compare_exchange_strong(untagged,tagged)) {
            break;
        }
    }
    /* read the flag and address fields */
    orc_ptr<Node*> tmpSibling = siblingAddr->load();

    /* make the sibling node a direct child of the ancestor node */
    res=successorAddr->compare_exchange_strong(successor,
        mixPtrFlgTg(tmpSibling.getUnmarked(),tmpSibling.getFlag(),false));

    return res;
}

/* to test rangeQuery */
// template <>
// optional<int> NatarajanTree<int,int>::get(int key){
//  int len=0;
//  auto x = rangeQuery(key-500,key,len,tid);
//  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
//  optional<int> res={};
//  SeekRecord* seekRecord=&(records[tid].ui);
//  Node* leaf=nullptr;
//  seek(key,tid);
//  leaf=getPtr(seekRecord->leaf);
//  if(nodeEqual(&keyNode,leaf)){
//      res = leaf->val;
//  }
//  return res;
// }

template <class K, class V>
std::optional<V> NatarajanTreeOrcGC<K,V>::get(K key){
    std::optional<V> res={};
    SeekRecord seekRecord;
    seek(key, seekRecord);
    if (keyEqual(key,seekRecord.leaf)) res = seekRecord.leaf->val;
    return res;
}

template <class K, class V>
std::optional<V> NatarajanTreeOrcGC<K,V>::put(K key, V val){
    std::optional<V> res={};
    SeekRecord seekRecord;

    orc_ptr<Node*> newInternal;
    orc_ptr<Node*> newLeaf = make_orc<Node>(key,val,nullptr,nullptr);//also to compare keys
    orc_ptr<Node*> parent;
    orc_ptr<Node*> leaf;
    orc_atomic<Node*>* childAddr=nullptr;

    while(true){
        seek(key, seekRecord);
        leaf = seekRecord.leaf;
        parent = seekRecord.parent;
        if(!nodeEqual(newLeaf,leaf)){//key does not exist
            /* obtain address of the child field to be modified */
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);

            /* create left and right leave of newInternal */
            orc_ptr<Node*> newLeft;
            orc_ptr<Node*> newRight;
            if (nodeLess(newLeaf,leaf)) {
                newLeft=newLeaf;
                newRight=leaf;
            } else {
                newLeft=leaf;
                newRight=newLeaf;
            }

            /* create newInternal */
            if(isInf(leaf)){
                int lev=getInfLevel(leaf);
                newInternal = make_orc<Node>(infK,defltV,newLeft,newRight,lev);
            }
            else
                newInternal = make_orc<Node>(std::max(key,leaf->key),defltV,newLeft,newRight);

            /* try to add the new nodes to the tree */
            orc_ptr<Node*> tmpExpected = leaf.getUnmarked();
            if (childAddr->compare_exchange_strong(tmpExpected,newInternal)) {
                res={};
                break;//insertion succeeds
            }
            else{//fails; help conflicting delete operation
                orc_ptr<Node*> tmpChild = childAddr->load();
                if(tmpChild.getUnmarked()==leaf.getUnmarked() && (tmpChild.getFlag()||tmpChild.getTag())){
                    /*
                     * address of the child has not changed
                     * and either the leaf node or its sibling
                     * has been flagged for deletion
                     */
                    cleanup(key, seekRecord);
                }
            }
        }
        else{//key exists, update and return old
            res=leaf->val;
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);
            if(childAddr->compare_exchange_strong(leaf,newLeaf)){
                break;
            }
        }
    }
    return res;
}

template <class K, class V>
bool NatarajanTreeOrcGC<K,V>::insert(K key, V val) {
    bool res=false;
    SeekRecord seekRecord;

    orc_ptr<Node*> newInternal;
    orc_ptr<Node*> newLeaf = make_orc<Node>(key,val,nullptr,nullptr);//also for comparing keys
    orc_atomic<Node*>* childAddr=nullptr;
    long iter = 0;
    while(true) {
        iter++;
        seek(key, seekRecord);
        seekRecord.leaf.unmark();
        seekRecord.parent.unmark();
        if (!nodeEqual(newLeaf,seekRecord.leaf)){//key does not exist
            /* obtain address of the child field to be modified */
            if(nodeLess(newLeaf,seekRecord.parent))
                childAddr = &(seekRecord.parent->left);
            else
                childAddr = &(seekRecord.parent->right);

            /* create left and right leave of newInternal */
            orc_ptr<Node*> newLeft;
            orc_ptr<Node*> newRight;
            if (nodeLess(newLeaf,seekRecord.leaf)) {
                newLeft = newLeaf;
                newRight = seekRecord.leaf;
            } else {
                newLeft = seekRecord.leaf;
                newRight = newLeaf;
            }

            /* create newInternal */
            if (isInf(seekRecord.leaf)) {
                int lev = getInfLevel(seekRecord.leaf);
                newInternal = make_orc<Node>(infK,defltV,newLeft,newRight,lev);
            } else {
                newInternal = make_orc<Node>(std::max(key,seekRecord.leaf->key),defltV,newLeft,newRight);
            }

            /* try to add the new nodes to the tree */
            if (childAddr->compare_exchange_strong(seekRecord.leaf,newInternal)){
                res=true;
                break;//insertion succeeds
            } else {  //fails; help conflicting delete operation
                //printf("CAS failed. was %p   expected %p\n", childAddr->load().ptr, tmpExpected.ptr);
                orc_ptr<Node*> tmpChild = childAddr->load();
                if (tmpChild.getUnmarked()==seekRecord.leaf.getUnmarked() && (tmpChild.getFlag()||tmpChild.getTag())){
                    /*
                     * address of the child has not changed
                     * and either the leaf node or its sibling
                     * has been flagged for deletion
                     */
                    cleanup(key, seekRecord);
                }
            }
        } else { //key exists, insertion fails
            res=false;
            break;
        }
    }
    return res;
}

template <class K, class V>
std::optional<V> NatarajanTreeOrcGC<K,V>::innerRemove(K key){
    //printf("innerRemove()\n");
    bool injecting = true;
    std::optional<V> res={};
    SeekRecord seekRecord;

    orc_ptr<Node*> keyNode = make_orc<Node>(key,defltV,nullptr,nullptr);//node to be compared
    //orc_ptr<Node*> parent;
    orc_ptr<Node*> leaf;
    orc_atomic<Node*>* childAddr=nullptr;

    while(true) {
        seek(key, seekRecord);
        /* obtain address of the child field to be modified */
        if (nodeLess(keyNode,seekRecord.parent))
            childAddr=&(seekRecord.parent->left);
        else
            childAddr=&(seekRecord.parent->right);

        if(injecting){
            /* injection mode: check if the key exists */
            leaf = seekRecord.leaf;
            if(!nodeEqual(keyNode,leaf)){//does not exist
                res={};
                break;
            }

            /* inject the delete operation into the tree */
            res = leaf->val;
            if (childAddr->compare_exchange_strong(leaf.getUnmarked(), mixPtrFlgTg(leaf.getUnmarked(),true,false))) {
                /* advance to cleanup mode to remove the leaf node */
                injecting=false;
                if (cleanup(key, seekRecord)) break;
            } else {
                orc_ptr<Node*> tmpChild=childAddr->load();
                if(tmpChild.getUnmarked()==leaf.getUnmarked() && (tmpChild.getFlag()||tmpChild.getTag())){
                    /*
                     * address of the child has not
                     * changed and either the leaf
                     * node or its sibling has been
                     * flagged for deletion
                     */
                    cleanup(key, seekRecord);
                }
            }
        }
        else{
            /* cleanup mode: check if flagged node still exists */
            if (seekRecord.leaf != leaf.getUnmarked()){
                /* leaf no longer in the tree */
                break;
            }
            else{
                /* leaf still in the tree; remove */
                if (cleanup(key, seekRecord)) break;
            }
        }
    }
    return res;
}

template <class K, class V>
std::optional<V> NatarajanTreeOrcGC<K,V>::replace(K key, V val){
    std::optional<V> res={};
    SeekRecord seekRecord;

    orc_ptr<Node*> newInternal;
    orc_ptr<Node*> newLeaf = make_orc<Node>(key,val,nullptr,nullptr);//also to compare keys

    orc_ptr<Node*> parent;
    orc_ptr<Node*> leaf;
    orc_atomic<Node*>* childAddr=nullptr;
    while(true){
        seek(key, seekRecord);
        parent = seekRecord.parent;
        leaf = seekRecord.leaf;
        if(!nodeEqual(newLeaf,leaf)){//key does not exist, replace fails
            res={};
            break;
        }
        else{//key exists, update and return old
            res=leaf->val;
            if(nodeLess(newLeaf,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);
            if(childAddr->compare_exchange_strong(leaf,newLeaf)){
                break;
            }
        }
    }
    return res;
}

template <class K, class V>
std::map<K, V> NatarajanTreeOrcGC<K,V>::rangeQuery(K key1, K key2, int& len){
    if(key1>key2) return {};
    Node k1{key1,defltV,nullptr,nullptr};//node to be compared
    Node k2{key2,defltV,nullptr,nullptr};//node to be compared

    orc_ptr<Node*> leaf = s->left;
    orc_ptr<Node*> current = leaf->left;

    std::map<K,V> res;
    if(current.getUnmarked() != nullptr)
        doRangeQuery(k1,k2,current,res);
    len=res.size();
    return res;
}

template <class K, class V>
void NatarajanTreeOrcGC<K,V>::doRangeQuery(Node& k1, Node& k2, orc_ptr<Node*>& root, std::map<K,V>& res){
    orc_ptr<Node*> left = root->left;
    orc_ptr<Node*> right = root->right;
    if(left.getUnamrked() == nullptr && right.getUnamrked() == nullptr) {
        if (nodeLessEqual(k1,root) && nodeLessEqual(root,&k2)){
            res.emplace(root->key,root->val);
        }
        return;
    }
    if(left.getUnamrked() != nullptr){
        if(nodeLess(&k1,root)){
            doRangeQuery(k1,k2,left,res);
        }
    }
    if(right!=nullptr){
        if(nodeLessEqual(root,&k2)){
            doRangeQuery(k1,k2,right,res);
        }
    }
    return;
}


// Wrappers for the "set" benchmarks
template <class K, class V>
bool NatarajanTreeOrcGC<K,V>::add(K key) {
    return insert(key,key);
}

template <class K, class V>
bool NatarajanTreeOrcGC<K,V>::remove(K key) {
    return innerRemove(key).has_value();
}

template <class K, class V>
bool NatarajanTreeOrcGC<K,V>::contains(K key) {
    return get(key).has_value();
}

// Not lock-free
template <class K, class V>
void NatarajanTreeOrcGC<K,V>::addAll(K** keys, const int size) {
    for (int i = 0; i < size; i++) add(*keys[i]);
}

