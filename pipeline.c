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

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

#include <jeffpc/error.h>
#include <jeffpc/val.h>
#include <jeffpc/mem.h>

#include "pipeline.h"
#include "utils.h"
#include "mangle.h"

static struct mem_cache *pipestage_cache;
static struct mem_cache *pipeline_cache;

void init_pipe_subsys(void)
{
	pipestage_cache = mem_cache_create("pipestage-cache",
					   sizeof(struct pipestage), 0);
	ASSERT(!IS_ERR(pipestage_cache));

	pipeline_cache = mem_cache_create("pipeline-cache",
					  sizeof(struct pipeline), 0);
	ASSERT(!IS_ERR(pipeline_cache));
}

static struct val *nop_fxn(struct val *val)
{
	return val;
}

static char *str_of_int(uint64_t v)
{
	return NULL;
}

static char *__urlescape_str(const char *in)
{
	static const char hd[16] = "0123456789ABCDEF";

	char *out, *tmp;
	int outlen;
	const char *s;

	outlen = strlen(in);

	for (s = in; *s; s++) {
		char c = *s;

		if (isalnum(c))
			continue;

		switch (c) {
			case ' ':
			case '-':
			case '_':
			case '.':
			case '~':
				continue;
		}

		/* this char will be escaped */
		outlen += 2;
	}

	out = malloc(outlen + 1);
	ASSERT(out);

	for (s = in, tmp = out; *s; s++, tmp++) {
		unsigned char c = *s;

		if (isalnum(c)) {
			*tmp = c;
			continue;
		}

		switch (c) {
			case ' ':
				*tmp = '+';
				continue;
			case '-':
			case '_':
			case '.':
			case '~':
				*tmp = c;
				continue;
		}

		/* this char will be escaped */
		tmp[0] = '%';
		tmp[1] = hd[c >> 4];
		tmp[2] = hd[c & 0xf];

		tmp += 2;
	}

	*tmp = '\0';

	return out;
}

static struct val *__escape(struct val *val, char *(*cvt)(const char*))
{
	char *out;

	out = NULL;

	switch (val->type) {
		case VT_INT:
			out = str_of_int(val->i);
			break;
		case VT_STR:
			out = cvt(str_cstr(val->str));
			break;
		case VT_NULL:
			val_putref(val);
			return VAL_DUP_CSTR("");
		case VT_SYM:
		case VT_CONS:
		case VT_BOOL:
		case VT_CHAR:
			panic("%s called with value type %d", __func__,
			      val->type);
	}

	val_putref(val);

	ASSERT(out);

	return VAL_ALLOC_CSTR(out);
}

static struct val *urlescape_fxn(struct val *val)
{
	return __escape(val, __urlescape_str);
}

static struct val *escape_fxn(struct val *val)
{
	return __escape(val, mangle_htmlescape);
}

static struct val *__datetime(struct val *val, const char *fmt)
{
	char buf[64];
	struct tm tm;
	time_t ts;

	ASSERT3U(val->type, ==, VT_INT);

	ts = val->i;
	gmtime_r(&ts, &tm);
	strftime(buf, sizeof(buf), fmt, &tm);

	val_putref(val);

	return VAL_DUP_CSTR(buf);
}

static struct val *time_fxn(struct val *val)
{
	return __datetime(val, "%H:%M");
}

static struct val *date_fxn(struct val *val)
{
	return __datetime(val, "%B %e, %Y");
}

static struct val *zulu_fxn(struct val *val)
{
	return __datetime(val, "%Y-%m-%dT%H:%M:%SZ");
}

static struct val *rfc822_fxn(struct val *val)
{
	return __datetime(val, "%a, %d %b %Y %H:%M:%S +0000");
}

static const struct pipestageinfo stages[] = {
	{ "urlescape", urlescape_fxn, },
	{ "escape", escape_fxn, },
	{ "time", time_fxn, },
	{ "date", date_fxn, },
	{ "zulu", zulu_fxn, },
	{ "rfc822", rfc822_fxn, },
	{ NULL, },
};

static const struct pipestageinfo nop = { "nop", nop_fxn, };

struct pipestage *pipestage_alloc(char *name)
{
	struct pipestage *pipe;
	int i;

	pipe = mem_cache_alloc(pipestage_cache);
	ASSERT(pipe);

	pipe->stage = &nop;

	for (i = 0; stages[i].name; i++) {
		if (!strcmp(name, stages[i].name)) {
			pipe->stage = &stages[i];
			break;
		}
	}

	return pipe;
}

struct pipeline *pipestage_to_pipeline(struct pipestage *stage)
{
	struct pipeline *line;

	line = mem_cache_alloc(pipeline_cache);
	ASSERT(line);

	list_create(&line->pipe, sizeof(struct pipestage),
		    offsetof(struct pipestage, pipe));

	pipeline_append(line, stage);

	return line;
}

void pipeline_append(struct pipeline *line, struct pipestage *stage)
{
	list_insert_tail(&line->pipe, stage);
}

void pipeline_destroy(struct pipeline *line)
{
	while (!list_is_empty(&line->pipe))
		mem_cache_free(pipestage_cache, list_remove_head(&line->pipe));

	mem_cache_free(pipeline_cache, line);
}
