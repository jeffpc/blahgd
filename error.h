#ifndef __ERROR_H
#define __ERROR_H

#include <stdio.h>
#include <stdlib.h>

#define NORETURN __attribute__((__noreturn__))

extern void __my_log(const char *fmt, ...);
extern void __my_assfail(const char *a, const char *f, int l) NORETURN;
extern void __my_assfail3(const char *a, uintmax_t lv, const char *op,
			  uintmax_t rv, const char *f, int l) NORETURN;

#define LOG(...)	__my_log(__VA_ARGS__)

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
