/*
 * Copyright (c) 2013-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdlib.h>

#include <jeffpc/jeffpc.h>
#include <jeffpc/error.h>
#include <jeffpc/str.h>
#include <jeffpc/val.h>

#include "parse.h"
#include "utils.h"
#include "math.h"
#include "file_cache.h"

static int onefile(struct post *post, char *ibuf, size_t len)
{
	struct parser_output x;
	int ret;

	x.req            = NULL;
	x.post           = post;
	x.input          = ibuf;
	x.len            = len;
	x.pos            = 0;
	x.lineno         = 0;
	x.table_nesting  = 0;
	x.texttt_nesting = 0;
	x.sc_title       = NULL;
	x.sc_pub         = NULL;
	x.sc_tags        = NULL;
	x.sc_cats        = NULL;
	x.sc_twitter_img = NULL;

	fmt3_lex_init(&x.scanner);
	fmt3_set_extra(&x, x.scanner);

	ret = fmt3_parse(&x);
	if (!ret) {
		printf("output:\n>>>%s<<<\n", str_cstr(x.stroutput));
		printf("sc_title:\n>>>%s<<<\n", str_cstr(x.sc_title));
		printf("sc_pub:\n>>>%s<<<\n", str_cstr(x.sc_pub));
		printf("sc_tags:\n");
		sexpr_dump_file(stdout, x.sc_tags, false);
		printf("\nsc_cats:\n");
		sexpr_dump_file(stdout, x.sc_cats, false);
		printf("\nsc_twitter_img:\n>>>%s<<<\n",
		       str_cstr(x.sc_twitter_img));
		str_putref(x.stroutput);
		str_putref(x.sc_title);
		str_putref(x.sc_pub);
		val_putref(x.sc_tags);
		val_putref(x.sc_cats);
		str_putref(x.sc_twitter_img);
	} else {
		fprintf(stderr, "failed to parse\n");
	}

	fmt3_lex_destroy(x.scanner);

	return ret;
}

int main(int argc, char **argv)
{
	struct post post;
	char *in;
	int i;
	int result;

	result = 0;

	ASSERT0(putenv("UMEM_DEBUG=default,verbose"));

	init_math(false);
	init_file_cache();

	for (i = 1; i < argc; i++) {
		in = read_file(argv[i]);
		ASSERT(!IS_ERR(in));

		if (onefile(&post, in, strlen(in)))
			result = 1;

		free(in);
	}

	return result;
}
