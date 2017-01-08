/* The following is the NetBSD libc qsort implementation modified in order to
 * support partial sorting of ranges for Redis.
 *
 * Copyright(C) 2009-2012 Salvatore Sanfilippo. All rights reserved.
 *
 * The original copyright notice follows. */


/*	$NetBSD: qsort.c,v 1.19 2009/01/30 23:38:44 lukem Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>

static inline char	*med3 (char *, char *, char *,
    int (*)(const void *, const void *));
static inline void	 swapfunc (char *, char *, size_t, int);

#define min(a, b)	(a) < (b) ? a : b

/* @4396 交换长度为n的parmi和parmj */
/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) { 		\
	size_t i = (n) / sizeof (TYPE); 		\
	TYPE *pi = (TYPE *)(void *)(parmi); 		\
	TYPE *pj = (TYPE *)(void *)(parmj); 		\
	do { 						\
		TYPE	t = *pi;			\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

/* @4396 2017-01-08 18:32:22
 *
 * 确定swaptype的值，用于快速交换
 *
 * a表示数组，es表示数组中元素的大小
 * 下面代码的简化版本是 (x || y) ? 2 : (z ? 0 : 1)
 * 如果((char *)a - (char *)0) % sizeof(long)等于0表示数组a与机器字节对齐
 *
 * (数组a字节对齐 || 数组元素不是sizeof(long)的倍数) --> 2
 * (数组a字节对齐 || 数组元素等于sizeof(long)) --> 0
 * (数组a字节对齐 || 数组元素是sizeof(long)的倍数) --> 1
 * (数组a不是字节对齐 || 无所谓) --> 其他(addr%sizeof(long))
 */
#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

/* @4396 根据交换类型交换长度为n的对象a和b */
static inline void
swapfunc(char *a, char *b, size_t n, int swaptype)
{
	/* @4396 0、1表示数组字节对齐且元素为sizeof(long)的倍数 */
	if (swaptype <= 1)
		swapcode(long, a, b, n)
	else
		swapcode(char, a, b, n)
}

/* @4396 交换对象a和b */
#define swap(a, b)						\
	if (swaptype == 0) {					\
		long t = *(long *)(void *)(a);			\
		*(long *)(void *)(a) = *(long *)(void *)(b);	\
		*(long *)(void *)(b) = t;			\
	} else							\
		swapfunc(a, b, es, swaptype)

/* @4396 交换数组a和b */
#define vecswap(a, b, n) if ((n) > 0) swapfunc((a), (b), (size_t)(n), swaptype)

/* @4396 取a、b、c中间的值 */
static inline char *
med3(char *a, char *b, char *c,
    int (*cmp) (const void *, const void *))
{

	return cmp(a, b) < 0 ?
	       (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
              :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

/* @4396 排序长度为n的数组a，排序区间l到m */
static void
_pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), void *lrange, void *rrange)
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	size_t d, r;
	int swaptype, cmp_result;

loop:	SWAPINIT(a, es);
	if (n < 7) {
		/* @4396 当数组元素个数小于7个时，直接使用冒泡排序 */
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *) a + (n / 2) * es;
	if (n > 7) {
		pl = (char *) a;
		pn = (char *) a + (n - 1) * es;
		if (n > 40) {
			/* @4396 2017-01-08 21:32:40
			 *
			 * 当数组元素个数大于40时，将数组分为8份
			 * pl等于左边1、2、3的中间值
			 * pm等于中间4、5、6的中间值
			 * pn等于右边6、7、8的中间值
			 */
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp);
			pm = med3(pm - d, pm, pm + d, cmp);
			pn = med3(pn - 2 * d, pn - d, pn, cmp);
		}
		/* @4396 pm取pl、pm、pn的中间值 */
		pm = med3(pl, pm, pn, cmp);
	}
	/* @4396 将pm放在数组的最左边 */
	swap(a, pm);
	/* @4396 pa、pb指向数组的第二个元素 */
	pa = pb = (char *) a + es;
	/* @4396 pc、pd指向数组的最后一个元素 */
	pc = pd = (char *) a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (cmp_result = cmp(pb, a)) <= 0) {
			if (cmp_result == 0) {
				/* @4396 如果pb等于pm(a[0])，将其依次放到数组左边 */
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (cmp_result = cmp(pc, a)) >= 0) {
			if (cmp_result == 0) {
				/* @4396 如果pb等于pm(a[0])，将其依次放到数组右边 */
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		pb += es;
		pc -= es;
	}
	/* @4396 2017-01-08 21:48:35
	 *
	 * 经过上面的for循环后
	 * [左, pa) == pm
	 * [pa, pb) < pm
	 * [pc, pd) > pm
	 * [pd, 右] == pm
	 * 其中
	 * pb == pc + 1
	 * pa <= pb
	 * pc >= pd
	 *
	 * == ... == < ... < > ... > ... == ... ==
	 */

	pn = (char *) a + n * es;
	r = min(pa - (char *) a, pb - pa);
	vecswap(a, pb - r, r);
	r = min((size_t)(pd - pc), pn - pd - es);
	vecswap(pb, pn - r, r);
	/* @4396 2017-01-08 22:05:50
	 *
	 * 经过上面的两次vecswap后
	 * 左边的全是小于pm的对象
	 * 中间的全是等于pm的对象
	 * 右边的全是大于pm的对象
	 * < ... < == ... == ... == > ... >
	 */
	if ((r = pb - pa) > es) {
		void *_l = a, *_r = ((unsigned char*)a)+r-1;
		/* @4396 存在小于pm的元素，且待排序区间在l、r之间 */
		if (!((lrange < _l && rrange < _l) ||
			(lrange > _r && rrange > _r)))
		    _pqsort(a, r / es, es, cmp, lrange, rrange);
        }
	if ((r = pd - pc) > es) {
		void *_l, *_r;

		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		n = r / es;

		_l = a;
		_r = ((unsigned char*)a)+r-1;
		/* @4396 存在大于pm的元素，且待排序区间在l、r之间 */
		if (!((lrange < _l && rrange < _l) ||
			(lrange > _r && rrange > _r)))
		    goto loop;
	}
/*	qsort(pn - r, r / es, es, cmp);*/
}

/* @4396 排序长度为n的数组a，取第l到m大小的值 */
void
pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), size_t lrange, size_t rrange)
{
    _pqsort(a,n,es,cmp,((unsigned char*)a)+(lrange*es),
                       ((unsigned char*)a)+((rrange+1)*es)-1);
}
