#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <umem.h>

#include "pipeline.h"
#include "error.h"
#include "utils.h"
#include "mangle.h"

static umem_cache_t *pipeline_cache;

void init_pipe_subsys()
{
	pipeline_cache = umem_cache_create("pipeline-cache",
					   sizeof(struct pipeline), 0, NULL,
					   NULL, NULL, NULL, NULL, 0);
	ASSERT(pipeline_cache);
}

static struct var_val *nop_fxn(struct var_val *v)
{
	return v;
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

static struct var_val *__escape(struct var_val *v, char *(*cvt)(char*))
{
	char *out;

	out = NULL;

	switch (v->type) {
		case VT_VARS:
			var_val_dump(v, 0, 10);
			ASSERT(0);
			break;
		case VT_NIL:
			out = xstrdup("");
			break;
		case VT_INT:
			out = str_of_int(v->i);
			break;
		case VT_STR:
			out = cvt(v->str);
			break;
	}

	ASSERT(out);

	v = malloc(sizeof(struct var_val));
	ASSERT(v);

	v->type = VT_STR;
	v->str  = out;

	return v;
}

static struct var_val *urlescape_fxn(struct var_val *v)
{
	return __escape(v, __urlescape_str);
}

static struct var_val *escape_fxn(struct var_val *v)
{
	return __escape(v, mangle_htmlescape);
}

static struct var_val *__datetime(struct var_val *v, const char *fmt)
{
	char buf[64];
	struct tm tm;
	time_t ts;

	ASSERT(v->type == VT_INT);

	ts = v->i;
	gmtime_r(&ts, &tm);
	strftime(buf, sizeof(buf), fmt, &tm);


	v = malloc(sizeof(struct var_val));
	ASSERT(v);

	v->type = VT_STR;
	v->str  = xstrdup(buf);
	ASSERT(v->str);

	return v;
}

static struct var_val *time_fxn(struct var_val *v)
{
	return __datetime(v, "%H:%M");
}

static struct var_val *date_fxn(struct var_val *v)
{
	return __datetime(v, "%B %e, %Y");
}

static struct var_val *zulu_fxn(struct var_val *v)
{
	return __datetime(v, "%Y-%m-%dT%H:%M:%SZ");
}

static struct var_val *rfc822_fxn(struct var_val *v)
{
	return __datetime(v, "%a, %d %b %Y %H:%M:%S +0000");
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
