#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "pipeline.h"

static struct var_val *nop_fxn(struct var_val *v)
{
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

static const struct pipestages stages[] = {
	{ "time", time_fxn, },
	{ "date", date_fxn, },
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
