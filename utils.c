/*
 * Copyright (c) 2011-2018 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"

#define HDD_START	0
#define HDD_1DOT	1
#define HDD_2DOT	2
#define HDD_OK		3

int hasdotdot(const char *path)
{
	int state = HDD_START;

	while(*path) {
		switch(*path) {
			case '.':
				if (state == HDD_START)
					state = HDD_1DOT;
				else if (state == HDD_1DOT)
					state = HDD_2DOT;
				else
					state = HDD_OK;
				break;
			case '/':
				if (state == HDD_2DOT)
					return 1; /* '/../' found */

				state = HDD_START;
				break;
			default:
				state = HDD_OK;
				break;
		}

		path++;
	}

	return (state == HDD_2DOT);
}

#define NARGS	5
char *concat5(char *a, char *b, char *c, char *d, char *e)
{
	char *src[NARGS] = { a, b, c, d, e, };
	size_t len[NARGS]; /* source lengths */
	size_t totallen; /* total output string len */
	unsigned tofree; /* what to free bitmap */
	char *ret, *out;
	int first, last; /* the first and last non-NULL index */
	int nsrc; /* number of non-NULL sources */
	int i, j;

	nsrc = 0;
	totallen = 0;
	first = -1;
	last = -1;
	tofree = 0x1f;

	for (i = 0; i < 5; i++) {
		len[i] = 0;

		if (src[i]) {
			len[i] = strlen(src[i]);

			last = i;
			if (first == -1)
				first = i;

			nsrc++;

			for (j = i + 1; j < 5; j++)
				if (src[i] == src[j])
					tofree &= ~(1 << i);
		}

		totallen += len[i];
	}

	if (nsrc == 1)
		return src[first];

	if (!totallen) {
		for (i = first; i < last; i++)
			if (tofree & (1 << i))
				free(src[i]);

		return xstrdup("");
	}

	ret = malloc(totallen + 1);
	ASSERT(ret);

	out = ret;

	for (i = first; i <= last; i++) {
		if (!src[i])
			continue;

		/* copy the source */
		strcpy(out, src[i]);

		/* advance the output */
		out += len[i];

		if (tofree & (1 << i))
			free(src[i]);
	}

	/*
	 * We're guaranteed that the above loop executed at least once, and
	 * therefore the output is null-terminated
	 */

	return ret;
}

time_t parse_time_cstr(const char *str)
{
	struct tm tm;

	if (!str)
		return 0;

	strptime(str, "%Y-%m-%d %H:%M", &tm);

	return mktime(&tm);
}
