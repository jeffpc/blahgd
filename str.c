#include <stdlib.h>

#include "str.h"
#include "val.h"
#include "utils.h"

struct str *str_dup(const char *s)
{
	return str_alloc(xstrdup(s));
}

struct str *str_alloc(char *s)
{
	struct str *str;

	str = malloc(sizeof(struct str));
	if (!str)
		return NULL;

	atomic_set(&str->refcnt, 1);
	str->str = s;

	return str;
}

size_t str_len(const struct str *s)
{
	return strlen(s->str);
}

struct str *str_cat5(struct str *a, struct str *b, struct str *c,
		     struct str *d, struct str *e)
{
#define NSTRS	5
	struct str *strs[NSTRS] = {a, b, c, d, e};
	size_t len[NSTRS];
	size_t totallen;
	struct str *ret;
	char *buf, *out;
	int i;

	totallen = 0;

	for (i = 0; i < NSTRS; i++) {
		if (!strs[i])
			continue;

		len[i] = strlen(strs[i]->str);

		totallen += len[i];
	}

	buf = malloc(totallen + 1);
	ASSERT(buf);

	out = buf;

	for (i = 0; i < NSTRS; i++) {
		if (!strs[i])
			continue;

		strcpy(out, strs[i]->str);

		out += len[i];

		str_putref(strs[i]);
	}

	ret = str_alloc(buf);
	ASSERT(ret);

	return ret;
}

void str_free(struct str *str)
{
	if (!str)
		return;

	ASSERT3U(atomic_read(&str->refcnt), ==, 0);

	free(str->str);
	free(str);
}

struct str *str_getref(struct str *str)
{
	if (!str)
		return NULL;

	ASSERT3U(atomic_read(&str->refcnt), >=, 1);

	atomic_inc(&str->refcnt);

	return str;
}

void str_putref(struct str *str)
{
	if (!str)
		return;

	ASSERT3S(atomic_read(&str->refcnt), >=, 1);

	if (!atomic_dec(&str->refcnt))
		str_free(str);
}
