/*
 * Copyright (c) 2011-2019 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#ifndef __UTILS_H
#define __UTILS_H

#include <sys/stat.h>
#include <string.h>

#include <jeffpc/error.h>
#include <jeffpc/int.h>
#include <jeffpc/val.h>
#include <jeffpc/io.h>
#include <jeffpc/time.h>
#include <jeffpc/rbtree.h>

extern int hasdotdot(const char *path);
extern char *concat5(char *a, char *b, char *c, char *d, char *e);
extern time_t parse_time_cstr(const char *str);

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

static inline time_t parse_time_str(struct str *str)
{
	time_t ret;

	ret = parse_time_cstr(str_cstr(str));

	str_putref(str);

	return ret;
}

#endif
