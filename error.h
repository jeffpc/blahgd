/*
 * Copyright (c) 2013-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ERROR_H
#define __ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_ERRNO	1023

static inline int PTR_ERR(void *ptr)
{
	return -(intptr_t) ptr;
}

static inline void *ERR_PTR(int err)
{
	return (void *)(intptr_t) -err;
}

static inline int IS_ERR(void *ptr)
{
	intptr_t err = (intptr_t) ptr;

	return (err < 0) && (err >= -MAX_ERRNO);
}

static inline void *ERR_CAST(void *ptr)
{
	return ptr;
}

#define NORETURN __attribute__((__noreturn__))

extern void __my_log(const char *fmt, ...);
extern void __my_assfail(const char *a, const char *f, int l) NORETURN;
extern void __my_assfail3(const char *a, uintmax_t lv, const char *op,
			  uintmax_t rv, const char *f, int l) NORETURN;

#define LOG(...)	__my_log(__VA_ARGS__)

#define ASSERT3P(l, op, r)						\
	do {								\
		uintptr_t lhs = (uintptr_t)(l);				\
		uintptr_t rhs = (uintptr_t)(r);				\
		if (!(lhs op rhs))					\
			__my_assfail3(#l " " #op " " #r, lhs, #op, rhs,	\
				      __FILE__, __LINE__);		\
	} while(0)

#define ASSERT3U(l, op, r)						\
	do {								\
		uint64_t lhs = (l);					\
		uint64_t rhs = (r);					\
		if (!(lhs op rhs))					\
			__my_assfail3(#l " " #op " " #r, lhs, #op, rhs,	\
				      __FILE__, __LINE__);		\
	} while(0)

#define ASSERT3S(l, op, r)						\
	do {								\
		int64_t lhs = (l);					\
		int64_t rhs = (r);					\
		if (!(lhs op rhs))					\
			__my_assfail3(#l " " #op " " #r, lhs, #op, rhs,	\
				      __FILE__, __LINE__);		\
	} while(0)

#define ASSERT(c)							\
	do {								\
		if (!(c))						\
			__my_assfail(#c, __FILE__, __LINE__);		\
	} while(0)

#define ASSERT0(c)	ASSERT3U((c), ==, 0)

#endif
