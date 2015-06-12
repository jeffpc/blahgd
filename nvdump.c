/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdio.h>
#include <libnvpair.h>

#include "nvl.h"
#include "utils.h"

static void dump_file(char *fname)
{
	nvlist_t *nvl;
	size_t len;
	char *buf;
	int ret;

	buf = read_file_len(fname, &len);
	if (!buf) {
		fprintf(stderr, "Error: could not read file: %s\n", fname);
		return;
	}

	ret = nvlist_unpack(buf, len, &nvl, 0);
	if (ret) {
		fprintf(stderr, "Error: failed to parse file: %s\n", fname);
		goto out;
	}

	nvl_dump(nvl);

	nvlist_free(nvl);

out:
	free(buf);
}

int main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
		dump_file(argv[i]);

	return 0;
}
