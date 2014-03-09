#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <umem.h>

#include "pipeline.h"
#include "error.h"
#include "utils.h"
#include "mangle.h"
#include "val.h"

static umem_cache_t *pipeline_cache;

void init_pipe_subsys()
{
	pipeline_cache = umem_cache_create("pipeline-cache",
					   sizeof(struct pipeline), 0, NULL,
					   NULL, NULL, NULL, NULL, 0);
	ASSERT(pipeline_cache);
}

static struct val *nop_fxn(struct val *val)
{
	return val;
}

static char *str_of_int(uint64_t v)
{
	return NULL;
}

static char *__urlescape_str(char *in)
{
	static const char hd[16] = "0123456789ABCDEF";

	char *out, *tmp;
	int outlen;
	char *s;

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

static struct val *__escape(struct val *val, char *(*cvt)(char*))
{
	char *out;

	out = NULL;

	switch (val->type) {
		case VT_INT:
			out = str_of_int(val->i);
			break;
		case VT_STR:
			out = cvt(val->str);
			break;
	}

	val_putref(val);

	ASSERT(out);

	return VAL_ALLOC_STR(out);
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

	return VAL_ALLOC_STR(xstrdup(buf));
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

static const struct pipestages stages[] = {
	{ "urlescape", urlescape_fxn, },
	{ "escape", escape_fxn, },
	{ "time", time_fxn, },
	{ "date", date_fxn, },
	{ "zulu", zulu_fxn, },
	{ "rfc822", rfc822_fxn, },
	{ NULL, },
};

static const struct pipestages nop = { "nop", nop_fxn, };

struct pipeline *pipestage(char *name)
{
	struct pipeline *pipe;
	int i;

	pipe = umem_cache_alloc(pipeline_cache, 0);
	ASSERT(pipe);

	INIT_LIST_HEAD(&pipe->pipe);
	pipe->stage = &nop;

	for (i = 0; stages[i].name; i++) {
		if (!strcmp(name, stages[i].name)) {
			pipe->stage = &stages[i];
			break;
		}
	}

	return pipe;
}

void pipeline_destroy(struct list_head *pipelist)
{
	struct pipeline *cur;
	struct pipeline *tmp;

	list_for_each_entry_safe(cur, tmp, pipelist, pipe) {
		umem_cache_free(pipeline_cache, cur);
	}
}
