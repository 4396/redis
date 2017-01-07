/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */

/* @4396 链表节点定义 */
typedef struct listNode {
    /* @4396 前一个节点、后一个节点、值 */
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

/* @4396 链表迭代器定义 */
typedef struct listIter {
    /* @4396 下一个节点 */
    listNode *next;
    /* @4396 方向：顺序0、倒序1 */
    int direction;
} listIter;

/* @4396 链表定义 */
typedef struct list {
    /* @4396 头节点、尾节点 */
    listNode *head;
    listNode *tail;
    /* @4396 针对节点值的复制、释放、匹配函数指针 */
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);
    /* @4396 链表长度(节点个数) */
    unsigned long len;
} list;

/* Functions implemented as macros */
/* @4396 链表长度 */
#define listLength(l) ((l)->len)
/* @4396 链表头节点 */
#define listFirst(l) ((l)->head)
/* @4396 链表尾节点 */
#define listLast(l) ((l)->tail)
/* @4396 上一个节点 */
#define listPrevNode(n) ((n)->prev)
/* @4396 下一个节点 */
#define listNextNode(n) ((n)->next)
/* @4396 节点的值 */
#define listNodeValue(n) ((n)->value)

/* @4396 设置链表节点值的复制、释放、匹配函数 */
#define listSetDupMethod(l,m) ((l)->dup = (m))
#define listSetFreeMethod(l,m) ((l)->free = (m))
#define listSetMatchMethod(l,m) ((l)->match = (m))

/* @4396 获取链表节点值的复制、释放、匹配函数 */
#define listGetDupMethod(l) ((l)->dup)
#define listGetFree(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
/* @4396 创建链表 */
list *listCreate(void);
/* @4396 释放链表 */
void listRelease(list *list);
/* @4396 向链表头部插入一个值 */
list *listAddNodeHead(list *list, void *value);
/* @4396 向链表尾部插入一个值 */
list *listAddNodeTail(list *list, void *value);
/* @4396 向链表插入一个值 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
/* @4396 删除链表中的一个节点 */
void listDelNode(list *list, listNode *node);
/* @4396 获取链表迭代器，direction：0顺序、1倒序 */
listIter *listGetIterator(list *list, int direction);
/* @4396 获取迭代器下一个节点 */
listNode *listNext(listIter *iter);
/* @4396 释放链表迭代器 */
void listReleaseIterator(listIter *iter);
/* @4396 复制链表 */
list *listDup(list *orig);
/* @4396 搜索链表中值为key的第一个节点 */
listNode *listSearchKey(list *list, void *key);
/* @4396 获取链表中index位置的节点 */
listNode *listIndex(list *list, long index);
/* @4396 将迭代器重置到头节点 */
void listRewind(list *list, listIter *li);
/* @4396 将迭代器重置到尾节点 */
void listRewindTail(list *list, listIter *li);
/* @4396 旋转链表 */
void listRotate(list *list);

/* Directions for iterators */
/* @4396 用来表示迭代器的方向 */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
