#ifndef __UTILS_H
#define __UTILS_H

#include <string.h>

#include "error.h"

extern int hasdotdot(char *path);
extern int xread(int fd, void *buf, size_t nbyte);

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
