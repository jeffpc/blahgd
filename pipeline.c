#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "pipeline.h"

static struct var_val *nop_fxn(struct var_val *v)
{
	return v;
}

static char *str_of_int(uint64_t v)
{
	return NULL;
}

static char *__urlescape(char *in)
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
	assert(out);

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

static struct var_val *urlescape_fxn(struct var_val *v)
{
	char *out;

	out = NULL;

	switch (v->type) {
		case VT_VARS:
			var_val_dump(v, 0, 10);
			assert(0);
			break;
		case VT_NIL:
			out = strdup("");
			break;
		case VT_INT:
			out = str_of_int(v->i);
			break;
		case VT_STR:
			out = __urlescape(v->str);
			break;
	}

	assert(out);

	v = malloc(sizeof(struct var_val));
	assert(v);

	v->type = VT_STR;
	v->str  = out;

	return v;
}

static struct var_val *__datetime(struct var_val *v, const char *fmt)
{
	char buf[64];
	struct tm tm;
	time_t ts;

	assert(v->type == VT_INT);

	ts = v->i;
	gmtime_r(&ts, &tm);
	strftime(buf, sizeof(buf), fmt, &tm);


	v = malloc(sizeof(struct var_val));
	assert(v);

	v->type = VT_STR;
	v->str  = strdup(buf);
	assert(v->str);

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

static const struct pipestages stages[] = {
	{ "urlescape", urlescape_fxn, },
	{ "time", time_fxn, },
	{ "date", date_fxn, },
	{ "zulu", zulu_fxn, },
	{ NULL, },
};

static const struct pipestages nop = { "nop", nop_fxn, };

struct pipeline *pipestage(char *name)
{
	struct pipeline *pipe;
	int i;

	pipe = malloc(sizeof(struct pipeline));
	assert(pipe);

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
