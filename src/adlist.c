/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* @4396 创建链表，失败时返回NULL */
/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
list *listCreate(void)
{
    struct list *list;

    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    /* @4396 将头尾节点置为NULL */
    list->head = list->tail = NULL;
    /* @4396 将长度置为0 */
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* @4396 释放链表，这个函数不会失败 */
/* Free the whole list.
 *
 * This function can't fail. */
void listRelease(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        /* @4396 先释放节点的值，再释放节点本身 */
        if (list->free) list->free(current->value);
        zfree(current);
        current = next;
    }
    /* @4396 释放链表本身 */
    zfree(list);
}

/* @4396 向链表头部插入一个值，当失败时返回NULL，成功时返回链表本身 */
/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    /* @4396 创建新节点，并赋值value */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        /* @4396 当链表为空时，将头尾节点均指向新节点 */
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        /* @4396 当链表不为空时，将新节点插入到开头，并改变头节点为本节点 */
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    /* @4396 将链表长度加一 */
    list->len++;
    return list;
}

/* @4396 向链表尾部插入一个值，当失败时返回NULL，成功时返回链表本身 */
/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    /* @4396 创建新节点，并赋值value */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        /* @4396 当链表为空时，将头尾节点均指向新节点 */
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        /* @4396 当链表不为空时，将新节点插入到最后，并改变尾节点为本节点 */
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    /* @4396 将链表长度加一 */
    list->len++;
    return list;
}

/* @4396 向链表插入一个值 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    /* @4396 创建新节点，并赋值value */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (after) {
        /* @4396 将新节点插入到旧节点后面 */
        node->prev = old_node;
        node->next = old_node->next;
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        /* @4396 将新节点插入到旧节点前面 */
        node->next = old_node;
        node->prev = old_node->prev;
        if (list->head == old_node) {
            list->head = node;
        }
    }
    /* @4396 将前节点的next指向新节点 */
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    /* @4396 将后节点的prev指向新节点 */
    if (node->next != NULL) {
        node->next->prev = node;
    }
    /* @4396 将链表长度加一 */
    list->len++;
    return list;
}

/* @4396 删除链表中的一个节点，这个函数不会失败 */
/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
void listDelNode(list *list, listNode *node)
{
    /* @4396 重新设置前后节点的指向关系 */
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    /* @4396 先释放节点的值，再释放节点本身 */
    if (list->free) list->free(node->value);
    zfree(node);
    /* @4396 将链表长度减一 */
    list->len--;
}

/* @4396 获取链表迭代器，direction：0顺序、1倒序，这个函数不会失败 */
/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    /* @4396 顺序时将next指向头节点，倒序时将next指向尾节点 */
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* @4396 释放链表迭代器 */
/* Release the iterator memory */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* @4396 将迭代器重置到头节点 */
/* Create an iterator in the list private iterator structure */
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

/* @4396 将迭代器重置到尾节点 */
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* @4396 2017-01-07 12:30:58
 *
 * 获取迭代器下一个节点，当没有节点时返回NULL
 *
 * 删除迭代器返回的节点是安全的，并不会导致迭代器异常
 *
 * 遍历链表：
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 */
/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* @4396 复制链表，返回新链表 */
/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    /* @4396 创建新链表 */
    if ((copy = listCreate()) == NULL)
        return NULL;
    /* @4396 复制值操作函数 */
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    /* @4396 将迭代器重置到开头，遍历链表 */
    listRewind(orig, &iter);
    while((node = listNext(&iter)) != NULL) {
        void *value;
        /* @4396 存在复制函数时使用复制函数复制节点值，否则直接指向旧节点的值 */
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                /* @4396 复制失败释放新链表 */
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;
        if (listAddNodeTail(copy, value) == NULL) {
            /* @4396 插入失败释放新链表 */
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* @4396 搜索链表中值为key的第一个节点 */
/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(list, &iter);
    while((node = listNext(&iter)) != NULL) {
        /* @4396 存在匹配函数时通过匹配函数判断，否则直接判断指针是否相同 */
        if (list->match) {
            if (list->match(node->value, key)) {
                return node;
            }
        } else {
            if (key == node->value) {
                return node;
            }
        }
    }
    return NULL;
}

/* @4396 2017-01-07 12:45:48
 *
 * 获取链表中index位置的节点
 *
 * 0表示第一个元素、1表示第2个元素。。。
 * 负数表示从尾节点开始，-1表示倒数第一个元素、-2表示倒数第二个元素。。。
 * 不在链表范围时返回NULL
 */
/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
listNode *listIndex(list *list, long index) {
    listNode *n;

    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
/* @4396 旋转链表，只是将尾节点插入到头节点之前，并未将整个链表全部翻转 */
void listRotate(list *list) {
    listNode *tail = list->tail;

    /* @4396 链表长度小于等于1时，不需要翻转 */
    if (listLength(list) <= 1) return;

    /* Detach current tail */
    list->tail = tail->prev;
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}
