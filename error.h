#ifndef __ERROR_H
#define __ERROR_H

#include <stdio.h>
#include <syslog.h>

#define LOG(...)	\
	syslog(LOG_LOCAL0 | LOG_CRIT, __VA_ARGS__)

static inline void __assfail(const char *assertion, const char *file,
			     int line, const char *func)
{
	char msg[512];

	snprintf(msg, sizeof(msg), "Assertion failed: %s, file %s, line %d, "
		 "function %s", assertion, file, line, func);

	LOG("%s", msg);
	fprintf(stderr, "%s\n", msg);

#if 0
	extern void __set_panicstr(const char *);
	__set_panicstr(msg);
#endif

	abort();
}

#define ASSERT3U(l, op, r)						\
	do {								\
		uint64_t lhs = (l);					\
		uint64_t rhs = (r);					\
		if (!(hs op rhs)) {					\
			char msg[128];					\
			snprintf(msg, sizeof(msg), "%s %s %s (%#lx %s %#lx)", \
				 #l, #op, #r, lhs, #op, rhs);		\
			__assfail(msg, __FILE__, __LINE__, __func__);	\
		}							\
	} while(0)

#define ASSERT(c)							\
	do {								\
		if (!(c))						\
			__assfail(#c, __FILE__, __LINE__, __func__);	\
	} while(0)

#endif
