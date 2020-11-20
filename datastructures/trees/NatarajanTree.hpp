/*

Copyright 2017 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Adapted from https://github.com/roghnin/Interval-Based-Reclamation/blob/master/src/rideables/NatarajanTree.hpp

Due to the usage of <optional>, this needs C++17 to compile
*/

#pragma once

#include <iostream>
#include <atomic>
#include <algorithm>
#include <map>
#include <optional>
#include "trackers/HazardPointers.hpp"


template <class K, class V, template<typename> class Reclaimer = HazardPointers>
class NatarajanTree {
private:
    /* structs*/
    struct Node : Reclaimer<Node>::BaseObj {
        int level;
        K key;
        V val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node() {};
        Node(K k, V v, Node* l, Node* r,int lev):level(lev),key(k),val(v),left(l),right(r) {};
        Node(K k, V v, Node* l, Node* r):level(-1),key(k),val(v),left(l),right(r) {};
    } __attribute__((aligned(128)));
    struct SeekRecord{
        Node* ancestor;
        Node* successor;
        Node* parent;
        Node* leaf;
    } __attribute__((aligned(128)));

    /* variables */
    Reclaimer<Node> hp {5};

    K infK{};
    V defltV{};
    Node* r;
    Node* s;
    SeekRecord* records;
    const size_t GET_POINTER_BITS = 0xfffffffffffffffc;//for machine 64-bit or less.

    /* helper functions */
    //flag and tags helpers
    inline Node* getPtr(Node* mptr){
        return (Node*) ((size_t)mptr & GET_POINTER_BITS);
    }
    inline bool getFlg(Node* mptr){
        return (bool)((size_t)mptr & 1);
    }
    inline bool getTg(Node* mptr){
        return (bool)((size_t)mptr & 2);
    }
    inline Node* mixPtrFlgTg(Node* ptr, bool flg, bool tg){
        return (Node*) ((size_t)ptr | flg | ((size_t)tg<<1));
    }
    //node comparison
    inline bool isInf(Node* n){
        return getInfLevel(n)!=-1;
    }
    inline int getInfLevel(Node* n){
        //0 for inf0, 1 for inf1, 2 for inf2, -1 for general val
        n=getPtr(n);
        return n->level;
    }
    inline bool nodeLess(Node* n1, Node* n2){
        n1=getPtr(n1);
        n2=getPtr(n2);
        int i1=getInfLevel(n1);
        int i2=getInfLevel(n2);
        return i1<i2 || (i1==-1&&i2==-1&&n1->key<n2->key);
    }
    inline bool nodeEqual(Node* n1, Node* n2){
        n1=getPtr(n1);
        n2=getPtr(n2);
        int i1=getInfLevel(n1);
        int i2=getInfLevel(n2);
        if(i1==-1&&i2==-1)
            return n1->key==n2->key;
        else
            return i1==i2;
    }
    inline bool nodeLessEqual(Node* n1, Node* n2){
        return !nodeLess(n2,n1);
    }

public:
    NatarajanTree() {
        r = new Node(infK,defltV,nullptr,nullptr,2);
        s = new Node(infK,defltV,nullptr,nullptr,1);
        r->right = new Node(infK,defltV,nullptr,nullptr,2);
        r->left = s;
        s->right = new Node(infK,defltV,nullptr,nullptr,1);
        s->left = new Node(infK,defltV,nullptr,nullptr,0);
        records = new SeekRecord[REGISTRY_MAX_THREADS]{};
    };

    ~NatarajanTree() {
        delete[] records;
    };

    static std::string className() { return "NatarajanTree-" + Reclaimer<Node>::className(); }

/*
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
*/
    void seek(K key) {
        /* initialize the seek record using sentinel nodes */
        Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
        SeekRecord* seekRecord = &(records[ThreadRegistry::getTID()]);
        seekRecord->ancestor = r;
        seekRecord->successor = hp.protect(1, &r->left);
        seekRecord->parent = hp.protect(2, &r->left);
        seekRecord->leaf = getPtr(hp.protect(3, &s->left));

        /* initialize other variables used in the traversal */
        Node* parentField = hp.protect(3, &seekRecord->parent->left);
        Node* currentField = hp.protect(4, &seekRecord->leaf->left);
        Node* current = getPtr(currentField);

        /* traverse the tree */
        while(current!=nullptr){
            /* check if the edge from the current parent node is tagged */
            if(!getTg(parentField)){
                /*
                 * found an untagged edge in the access path;
                 * advance ancestor and successor pointers.
                 */
                seekRecord->ancestor=seekRecord->parent;
                hp.swapPtrs(0, 1);
                seekRecord->successor=seekRecord->leaf;
                hp.swapPtrs(1, 3);
            }

            /* advance parent and leaf pointers */
            seekRecord->parent = seekRecord->leaf;
            hp.swapPtrs(2, 3);
            seekRecord->leaf = current;
            hp.swapPtrs(3, 4);

            /* update other variables used in traversal */
            parentField=currentField;
            if(nodeLess(&keyNode,current)){
                currentField = hp.protect(4, &current->left);
            }
            else{
                currentField = hp.protect(4, &current->right);
            }
            current=getPtr(currentField);
        }
        /* traversal complete */
        return;
    }


    bool cleanup(K key) {
        Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
        bool res=false;

        /* retrieve addresses stored in seek record */
        SeekRecord* seekRecord=&(records[ThreadRegistry::getTID()]);
        Node* ancestor=getPtr(seekRecord->ancestor);
        Node* successor=getPtr(seekRecord->successor);
        Node* parent=getPtr(seekRecord->parent);
        Node* leaf=getPtr(seekRecord->leaf);

        std::atomic<Node*>* successorAddr=nullptr;
        std::atomic<Node*>* childAddr=nullptr;
        std::atomic<Node*>* siblingAddr=nullptr;

        /* obtain address of field of ancestor node that will be modified */
        if(nodeLess(&keyNode,ancestor))
            successorAddr=&(ancestor->left);
        else
            successorAddr=&(ancestor->right);

        /* obtain addresses of child fields of parent node */
        if(nodeLess(&keyNode,parent)){
            childAddr=&(parent->left);
            siblingAddr=&(parent->right);
        }
        else{
            childAddr=&(parent->right);
            siblingAddr=&(parent->left);
        }
        Node* tmpChild=childAddr->load(std::memory_order_acquire);
        if(!getFlg(tmpChild)){
            /* the leaf is not flagged, thus sibling node should be flagged */
            tmpChild=siblingAddr->load(std::memory_order_acquire);
            /* switch the sibling address */
            siblingAddr=childAddr;
        }

        /* use TAS to tag sibling edge */
        while(true){
            Node* untagged=siblingAddr->load(std::memory_order_acquire);
            Node* tagged=mixPtrFlgTg(getPtr(untagged),getFlg(untagged),true);
            if(siblingAddr->compare_exchange_strong(untagged,tagged,std::memory_order_acq_rel)){
                break;
            }
        }
        /* read the flag and address fields */
        Node* tmpSibling=siblingAddr->load(std::memory_order_acquire);

        /* make the sibling node a direct child of the ancestor node */
        res=successorAddr->compare_exchange_strong(successor,
            mixPtrFlgTg(getPtr(tmpSibling),getFlg(tmpSibling),false),
            std::memory_order_acq_rel);

        if(res==true){
            hp.retire(getPtr(tmpChild));
            hp.retire(successor);
        }
        return res;
    }


    /* to test rangeQuery */
    // template <>
    // optional<int> NatarajanTree<int,int>::get(int key, int tid){
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
    std::optional<V> get(K key){
        Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
        std::optional<V> res={};
        SeekRecord* seekRecord=&(records[ThreadRegistry::getTID()]);
        Node* leaf=nullptr;
        seek(key);
        leaf=getPtr(seekRecord->leaf);
        if(nodeEqual(&keyNode,leaf)){
            res = leaf->val;
        }
        hp.clear();
        return res;
    }


    std::optional<V> put(K key, V val) {
        std::optional<V> res={};
        SeekRecord* seekRecord=&(records[ThreadRegistry::getTID()]);

        Node* newInternal=nullptr;
        Node* newLeaf = new Node(key,val,nullptr,nullptr);//also to compare keys

        Node* parent=nullptr;
        Node* leaf=nullptr;
        std::atomic<Node*>* childAddr=nullptr;

        while(true){
            seek(key);
            leaf=getPtr(seekRecord->leaf);
            parent=getPtr(seekRecord->parent);
            if(!nodeEqual(newLeaf,leaf)){//key does not exist
                /* obtain address of the child field to be modified */
                if(nodeLess(newLeaf,parent))
                    childAddr=&(parent->left);
                else
                    childAddr=&(parent->right);

                /* create left and right leave of newInternal */
                Node* newLeft=nullptr;
                Node* newRight=nullptr;
                if(nodeLess(newLeaf,leaf)){
                    newLeft=newLeaf;
                    newRight=leaf;
                }
                else{
                    newLeft=leaf;
                    newRight=newLeaf;
                }

                /* create newInternal */
                if(isInf(leaf)){
                    int lev=getInfLevel(leaf);
                    newInternal = new Node(infK,defltV,newLeft,newRight,lev);
                }
                else
                    newInternal = new Node(std::max(key,leaf->key),defltV,newLeft,newRight);

                /* try to add the new nodes to the tree */
                Node* tmpExpected=getPtr(leaf);
                if(childAddr->compare_exchange_strong(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
                    res={};
                    break;//insertion succeeds
                }
                else{//fails; help conflicting delete operation
                    delete newInternal;
                    Node* tmpChild=childAddr->load(std::memory_order_acquire);
                    if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
                        /*
                         * address of the child has not changed
                         * and either the leaf node or its sibling
                         * has been flagged for deletion
                         */
                        cleanup(key);
                    }
                }
            }
            else{//key exists, update and return old
                res=leaf->val;
                if(nodeLess(newLeaf,parent))
                    childAddr=&(parent->left);
                else
                    childAddr=&(parent->right);
                if(childAddr->compare_exchange_strong(leaf,newLeaf,std::memory_order_acq_rel)){
                    hp.retire(leaf);
                    break;
                }
            }
        }
        hp.clear();
        return res;
    }


    bool insert(K key, V val) {
        bool res=false;
        SeekRecord* seekRecord=&(records[ThreadRegistry::getTID()]);

        Node* newInternal=nullptr;
        Node* newLeaf = new Node(key,val,nullptr,nullptr);//also for comparing keys

        Node* parent=nullptr;
        Node* leaf=nullptr;
        std::atomic<Node*>* childAddr=nullptr;
        while(true){
            seek(key);
            leaf=getPtr(seekRecord->leaf);
            parent=getPtr(seekRecord->parent);
            if(!nodeEqual(newLeaf,leaf)){//key does not exist
                /* obtain address of the child field to be modified */
                if(nodeLess(newLeaf,parent))
                    childAddr=&(parent->left);
                else
                    childAddr=&(parent->right);

                /* create left and right leave of newInternal */
                Node* newLeft=nullptr;
                Node* newRight=nullptr;
                if(nodeLess(newLeaf,leaf)){
                    newLeft=newLeaf;
                    newRight=leaf;
                }
                else{
                    newLeft=leaf;
                    newRight=newLeaf;
                }

                /* create newInternal */
                if(isInf(leaf)){
                    int lev=getInfLevel(leaf);
                    newInternal = new Node(infK,defltV,newLeft,newRight,lev);
                }
                else
                    newInternal = new Node(std::max(key,leaf->key),defltV,newLeft,newRight);

                /* try to add the new nodes to the tree */
                Node* tmpExpected=getPtr(leaf);
                if(childAddr->compare_exchange_strong(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
                    res=true;
                    break;//insertion succeeds
                }
                else{//fails; help conflicting delete operation
                    delete newInternal;
                    Node* tmpChild=childAddr->load(std::memory_order_acquire);
                    if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
                        /*
                         * address of the child has not changed
                         * and either the leaf node or its sibling
                         * has been flagged for deletion
                         */
                        cleanup(key);
                    }
                }
            }
            else{//key exists, insertion fails
                delete newLeaf;
                res=false;
                break;
            }
        }
        hp.clear();
        return res;
    }

    std::optional<V> innerRemove(K key) {
        bool injecting = true;
        std::optional<V> res={};
        SeekRecord* seekRecord=&(records[ThreadRegistry::getTID()]);

        Node keyNode{key,defltV,nullptr,nullptr};//node to be compared

        Node* parent=nullptr;
        Node* leaf=nullptr;
        std::atomic<Node*>* childAddr=nullptr;
        while(true){
            seek(key);
            parent=getPtr(seekRecord->parent);
            /* obtain address of the child field to be modified */
            if(nodeLess(&keyNode,parent))
                childAddr=&(parent->left);
            else
                childAddr=&(parent->right);

            if(injecting){
                /* injection mode: check if the key exists */
                leaf=getPtr(seekRecord->leaf);
                if(!nodeEqual(leaf,&keyNode)){//does not exist
                    res={};
                    break;
                }

                /* inject the delete operation into the tree */
                Node* tmpExpected=getPtr(leaf);
                res=leaf->val;
                if(childAddr->compare_exchange_strong(tmpExpected,
                    mixPtrFlgTg(tmpExpected,true,false), std::memory_order_acq_rel)){
                    /* advance to cleanup mode to remove the leaf node */
                    injecting=false;
                    if(cleanup(key)) break;
                }
                else{
                    Node* tmpChild=childAddr->load(std::memory_order_acquire);
                    if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
                        /*
                         * address of the child has not
                         * changed and either the leaf
                         * node or its sibling has been
                         * flagged for deletion
                         */
                        cleanup(key);
                    }
                }
            }
            else{
                /* cleanup mode: check if flagged node still exists */
                if(seekRecord->leaf!=leaf){
                    /* leaf no longer in the tree */
                    break;
                }
                else{
                    /* leaf still in the tree; remove */
                    if(cleanup(key)) break;
                }
            }
        }
        hp.clear();
        return res;
    }

    std::optional<V> replace(K key, V val){
        std::optional<V> res={};
        SeekRecord* seekRecord=&(records[ThreadRegistry::getTID()]);

        Node* newInternal=nullptr;
        Node* newLeaf = new Node(key,val,nullptr,nullptr);//also to compare keys

        Node* parent=nullptr;
        Node* leaf=nullptr;
        std::atomic<Node*>* childAddr=nullptr;
        while(true){
            seek(key);
            parent=getPtr(seekRecord->parent);
            leaf=getPtr(seekRecord->leaf);
            if(!nodeEqual(newLeaf,leaf)){//key does not exist, replace fails
                delete newLeaf;
                res={};
                break;
            }
            else{//key exists, update and return old
                res=leaf->val;
                if(nodeLess(newLeaf,parent))
                    childAddr=&(parent->left);
                else
                    childAddr=&(parent->right);
                if(childAddr->compare_exchange_strong(leaf,newLeaf,std::memory_order_acq_rel)){
                    hp.retire(leaf);
                    break;
                }
            }
        }
        hp.clear();
        return res;
    }

    std::map<K, V> rangeQuery(K key1, K key2, int& len){
        //NOT HP-like GC safe.
        if(key1>key2) return {};
        Node k1{key1,defltV,nullptr,nullptr};//node to be compared
        Node k2{key2,defltV,nullptr,nullptr};//node to be compared

        Node* leaf = getPtr(hp.protect(0, &s->left));
        Node* current = getPtr(hp.protect(1, &leaf->left));

        std::map<K,V> res;
        if(current!=nullptr)
            doRangeQuery(k1,k2,current,res);
        len=res.size();
        return res;
    }

    void doRangeQuery(Node& k1, Node& k2, Node* root, std::map<K,V>& res){
        Node* left = getPtr(hp.protect(2, &root->left));
        Node* right = getPtr(hp.protect(3, &root->right));
        if(left==nullptr&&right==nullptr){
            if(nodeLessEqual(&k1,root)&&nodeLessEqual(root,&k2)){
                res.emplace(root->key,root->val);
            }
            return;
        }
        if(left!=nullptr){
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
    bool add(K key) {
        return insert(key,key);
    }

    bool remove(K key) {
        return innerRemove(key).has_value();
    }

    bool contains(K key) {
        return get(key).has_value();
    }

    // Not lock-free
    void addAll(K** keys, const int size) {
        for (int i = 0; i < size; i++) add(*keys[i]);
    }
};





