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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "render.h"
#include "parse.h"
#include "error.h"
#include "file_cache.h"

char *render_page(struct req *req, char *str)
{
	struct parser_output x;

	x.req   = req;
	x.post  = NULL;
	x.input = str;
	x.len   = strlen(str);
	x.pos   = 0;

	x.cond_stack_use = -1;

	tmpl_lex_init(&x.scanner);
	tmpl_set_extra(&x, x.scanner);

	ASSERT(tmpl_parse(&x) == 0);

	tmpl_lex_destroy(x.scanner);

	return x.output;
}

char *render_template(struct req *req, const char *tmpl)
{
	char path[FILENAME_MAX];
	struct str *raw;
	char *out;

	snprintf(path, sizeof(path), "templates/%s/%s.tmpl", req->fmt, tmpl);

	raw = file_cache_get_cb(path, revalidate_all_posts, NULL);
	if (!raw)
		return NULL;

	out = render_page(req, raw->str);

	str_putref(raw);

	return out;
}
