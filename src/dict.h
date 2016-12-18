/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* @4396 当变量没有使用时，可以使用该宏避免编译警告 */
/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

/* @4396 用来表示字典中的一个元素，键值对结构 */
typedef struct dictEntry {
    /* @4396 字典的键 */
    void *key;
    /* @4396 使用联合来表示值，c语言的惯用手法 */
    union {
        /* @4396 复杂类型，使用指针 */
        void *val;
        /* @4396 无符号整型 */
        uint64_t u64;
        /* @4396 有符号整型 */
        int64_t s64;
        /* @4396 双精度浮点型 */
        double d;
    } v;
    /* @4396 用来解决hash冲突，hash相同时用单链表串连 */
    struct dictEntry *next;
} dictEntry;

/* @4396 字典的基础函数，定义函数指针用来实现多态 */
typedef struct dictType {
    /* @4396 函数指针，计算key的hash值 */
    unsigned int (*hashFunction)(const void *key);
    /* @4396 函数指针，复制一个key */
    void *(*keyDup)(void *privdata, const void *key);
    /* @4396 函数指针，复制一个value */
    void *(*valDup)(void *privdata, const void *obj);
    /* @4396 函数指针，比较key1和key2的大小 */
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    /* @4396 函数指针，销毁一个key */
    void (*keyDestructor)(void *privdata, void *key);
    /* @4396 函数指针，复制一个value */
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* @4396 哈希表的定义 */
/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct dictht {
    /* @4396 用来存储所有的字典元素 */
    dictEntry **table;
    /* @4396 table的大小，2的倍数 */
    unsigned long size;
    /* @4396 size的掩码，等于size-1 */
    unsigned long sizemask;
    /* @4396 已使用的个数 */
    unsigned long used;
} dictht;

/* @4396 字典的定义 */
typedef struct dict {
    /* @4396 基础操作函数指针集 */
    dictType *type;
    /* @4396 用作基础函数的私有数据 */
    void *privdata;
    /* @4396 2016-12-18 18:24:00
     * 
     * 字典中用了两个hash表，主要是因为扩缩容时，需要重新计算所有元素的hash
     * 而字典元素很多的时候，重建一次耗时会比较久，所以分批计算会比较好
     * 这个时候用一个变量rehashidx来标识计算到哪一个位置了
     * 重新计算好的字典元素会从ht[0]转移到ht[1]中
     * 当全部计算/转移完毕后，再互换ht[0]与ht[1]
     * 期间的查询等操作需同时查询ht[0]和ht[1]
     * 而插入等操作则直接插入到ht[1]中
     *
     * 另外，当rehashidx等于-1时，表示没有在重建hash表
     */
    dictht ht[2];
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    int iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
typedef struct dictIterator {
    dict *d;
    long index;
    int table, safe;
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

/* @4396 哈希表的初始大小 */
/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
/* @4396 销毁/释放字典元素的值 */
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

/* @4396 给字典元素赋值，当复制函数可用时会复制产生一个新对象 */
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

/* @4396 将字典元素的值设置为有符号整型数值 */
#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

/* @4396 将字典元素的值设置为无符号整型数值 */
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

/* @4396 将字典元素的值设置为双精度浮点型数值 */
#define dictSetDoubleVal(entry, _val_) \
    do { entry->v.d = _val_; } while(0)

/* @4396 销毁/释放字典元素的键 */
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

/* @4396 给字典元素设置键，当复制函数可用时会复制产生一个新对象 */
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

/* @4396 比较字典中两个元素键的大小，当比较函数可用时通过比较函数进行比较 */
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key);
int dictReplace(dict *d, void *key, void *val);
dictEntry *dictReplaceRaw(dict *d, void *key);
int dictDelete(dict *d, const void *key);
int dictDeleteNoFree(dict *d, const void *key);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void dictGetStats(char *buf, size_t bufsize, dict *d);
unsigned int dictGenHashFunction(const void *key, int len);
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(unsigned int initval);
unsigned int dictGetHashFunctionSeed(void);
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
