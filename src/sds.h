/* SDSLib 2.0 -- A C dynamic strings library
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
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

#ifndef __SDS_H
#define __SDS_H

#define SDS_MAX_PREALLOC (1024*1024)

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

typedef char *sds;

/* Note: sdshdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
struct __attribute__ ((__packed__)) sdshdr5 {
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

/* @4396 2016-12-17 11:05:10
 * 
 * hdr是header的缩写，sdshdrN用来记录sds头信息
 * 所有的sds头结构体的最后两个字段都是flags和buf[]
 * 对于sdshdr8、16、32、64只是len和alloc的类型区别
 * sds就是一个char*，也等价于sdshdrN中的最后一个字段buf
 *
 * 理解s[-1]，s是一个sds类型，也就是char*，也是sdshdrN的buf字段
 * 那么s[-1] == sdshdrN->buf[-1] == sdshdrN->buf - 1 == sdshdrN->flags
 * 所以s[-1]就是在取sds的具体类型flags
 */

#define SDS_TYPE_5  0
#define SDS_TYPE_8  1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4
#define SDS_TYPE_MASK 7
#define SDS_TYPE_BITS 3
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (void*)((s)-(sizeof(struct sdshdr##T)));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))
#define SDS_TYPE_5_LEN(f) ((f)>>SDS_TYPE_BITS)

/* @4396 2016-12-17 11:48:38
 *
 * SDS_TYPE_MASK等于7，2进制表示就是0x111，正好3位
 * 使用sdshdrN->flags的低3位来表示sds类型
 * 其中sdshdr5->flags的高5位用来表示字符串长度
 * 其他类型的sds分别由相应类型的len表示字符串长度
 * 
 * sdshdrN所能表示字符串的最大长度就是2^N - 1
 */

/* 取sds的字符串长度 */
static inline size_t sdslen(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->len;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->len;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->len;
    }
    return 0;
}

/* 取sds的可用空间，SDS_TYPE_5可用空间始终为0 */
static inline size_t sdsavail(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            return 0;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

/* 设置sds的字符串长度 */
static inline void sdssetlen(sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                /* fp表示flags pointer，SDS_TYPE_5的flags由低3位的类型与高5位的长度组成 */
                unsigned char *fp = ((unsigned char*)s)-1;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len = newlen;
            break;
    }
}

/* 将sds的字符串长度加上inc */
static inline void sdsinclen(sds s, size_t inc) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                /* fp表示flags pointer，SDS_TYPE_5的flags由低3位的类型与高5位的长度组成 */
                unsigned char *fp = ((unsigned char*)s)-1;
                unsigned char newlen = SDS_TYPE_5_LEN(flags)+inc;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len += inc;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len += inc;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len += inc;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len += inc;
            break;
    }
}

/* 获取sds的字符串(分配)空间大小 */
/* sdsalloc() = sdsavail() + sdslen() */
static inline size_t sdsalloc(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->alloc;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->alloc;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->alloc;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->alloc;
    }
    return 0;
}

/* 设置sds字符串(分配)空间大小 */
static inline void sdssetalloc(sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->alloc = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->alloc = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->alloc = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

/* 根据长度为initlen的init数据创建一个sds对象 */
sds sdsnewlen(const void *init, size_t initlen);
/* 根据字符串init创建一个sds对象 */
sds sdsnew(const char *init);
/* 创建一个空的sds对象 */
sds sdsempty(void);
/* 从sds对象s复制一个新的sds对象 */
sds sdsdup(const sds s);
/* 销毁/释放sds对象s */
void sdsfree(sds s);
/* 将sds对象s的空间增长到len大小，并将新增的空间全部置为0 */
sds sdsgrowzero(sds s, size_t len);
/* 向sds对象s尾部追加长度为len的t内容 */
sds sdscatlen(sds s, const void *t, size_t len);
/* 向sds对象s尾部追加字符串t */
sds sdscat(sds s, const char *t);
/* 向sds对象s尾部追加另一个sds对象t */
sds sdscatsds(sds s, const sds t);
/* 向sds对象s拷贝一个长度为len的字符串t */
sds sdscpylen(sds s, const char *t, size_t len);
/* 向sds对象s拷贝一个字符串t */
sds sdscpy(sds s, const char *t);

/* 向sds对象s尾部添加一个待格式化的数据 */
sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

/* 向sds对象s尾部添加一个待格式化的数据 */
sds sdscatfmt(sds s, char const *fmt, ...);
/* 去掉sds对象s首尾的字符(cset中含有的字符) */
sds sdstrim(sds s, const char *cset);
/* 截取sds对象s中的一段[start, end] */
void sdsrange(sds s, int start, int end);
/* 更新sds对象s的字符串长度 */
void sdsupdatelen(sds s);
/* 将sds对象s清空，修改字符串长度为0，并以\0结尾 */
void sdsclear(sds s);
/* 比较sds对象s1和s2，当s1>s2时返回正数，当s1<s2时返回负数，相等时返回0 */
int sdscmp(const sds s1, const sds s2);
/* 将字符串s按字符串sep拆分成若干sds对象 */
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);
/* 销毁/释放sds数组tokens的空间 */
void sdsfreesplitres(sds *tokens, int count);
/* 将sds对象s中的字符全部转为小写 */
void sdstolower(sds s);
/* 将sds对象s中的字符全部转为大写 */
void sdstoupper(sds s);
/* 从long long型数值创建一个sds对象 */
sds sdsfromlonglong(long long value);
/* 将长度为len的字符串p追加到sds对象s中，会保留转义符 */
sds sdscatrepr(sds s, const char *p, size_t len);
/* 拆分命令行参数line，返回sds数组和个数argc */
sds *sdssplitargs(const char *line, int *argc);
/* 将sds对象s中含有from中的字符替换成to中的字符 */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
/* 将字符串数组argv加上字符串sep拼接成一个sds对象 */
sds sdsjoin(char **argv, int argc, char *sep);
/* 将sds对象数组argv加上字符串sep拼接成一个sds对象 */
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen);

/* Low level functions exposed to the user API */
/* 给sds对象s扩容，以便能够容纳下新增addlen大小的数据 */
sds sdsMakeRoomFor(sds s, size_t addlen);
/* 将sds对象s的字符串长度增大incr，incr也可以是负数，同时将最后一位置为\0 */
void sdsIncrLen(sds s, int incr);
/* 给sds对象s缩容，将sds剩余的可用空间全部去掉 */
sds sdsRemoveFreeSpace(sds s);
/* 返回sds对象s分配的大小，包含头大小、字符串长度、剩余空间大小和\0结束符 */
size_t sdsAllocSize(sds s);
/* 返回sds对象s的指针头，也就是sdshdrN的指针 */
void *sdsAllocPtr(sds s);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);

#ifdef REDIS_TEST
int sdsTest(int argc, char *argv[]);
#endif

#endif
