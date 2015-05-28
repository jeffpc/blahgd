#ifndef __UTILS_H
#define __UTILS_H

#include <sys/sysmacros.h>
#include <string.h>
#include <errno.h>

#include "error.h"

#define ARRAY_LEN(a)		(sizeof(a) / sizeof(a[0]))

extern int hasdotdot(const char *path);
extern int xread(int fd, void *buf, size_t nbyte);
extern int xwrite(int fd, const void *buf, size_t nbyte);
extern char *read_file_len(const char *fname, size_t *len);
extern int write_file(const char *fname, const char *data, size_t len);
extern char *concat5(char *a, char *b, char *c, char *d, char *e);

#define concat4(a, b, c, d)	concat5((a), (b), (c), (d), NULL)
#define concat3(a, b, c)	concat5((a), (b), (c), NULL, NULL)
#define concat(a, b)		concat5((a), (b), NULL, NULL, NULL)

#define S(x)			xstrdup(x)

static inline char *xstrdup_def(const char *s, const char *def)
{
	char *ret;

	ASSERT(def);

	ret = strdup(s ? s : def);
	ASSERT(ret);

	return ret;
}

static inline char *xstrdup(const char *s)
{
	return xstrdup_def(s, "");
}

#define STR_TO_INT(size, imax)						\
static inline int __str2u##size(const char *restrict s,			\
				uint##size##_t *i,			\
				int base)				\
{									\
	char *endptr;							\
	uint64_t tmp;							\
									\
	*i = 0;								\
									\
	errno = 0;							\
	tmp = strtoull(s, &endptr, base);				\
									\
	if (errno)							\
		return errno;						\
									\
	if (endptr == s)						\
		return EINVAL;						\
									\
	if (tmp > imax)							\
		return ERANGE;						\
									\
	*i = tmp;							\
	return 0;							\
}

STR_TO_INT(16, 0x000000000000fffful)
STR_TO_INT(32, 0x00000000fffffffful)
STR_TO_INT(64, 0xfffffffffffffffful)

#undef STR_TO_INT

#define str2u64(s, i)	__str2u64((s), (i), 10)
#define str2u32(s, i)	__str2u32((s), (i), 10)
#define str2u16(s, i)	__str2u16((s), (i), 10)

static inline uint64_t gettime(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static inline char *read_file(const char *fname)
{
	return read_file_len(fname, NULL);
}

#endif
