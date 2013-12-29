#ifndef __UTILS_H
#define __UTILS_H

#include <string.h>

#include "error.h"

#define ARRAY_LEN(a)		(sizeof(a) / sizeof(a[0]))

extern int hasdotdot(char *path);
extern int xread(int fd, void *buf, size_t nbyte);
extern int xwrite(int fd, const void *buf, size_t nbyte);
extern char *read_file(const char *fname);
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

#endif
