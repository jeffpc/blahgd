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

	str->refcnt = 1;
	str->str = s;

	return str;
}

struct str *str_cat5(struct str *a, struct str *b, struct str *c,
		     struct str *d, struct str *e)
{
#define NSTRS	5
	struct str *strs[NSTRS] = {a, b, c, d, e};
	struct str *out;
	size_t len;
	char *buf;
	int i;

	len = 0;

	for (i = 0; i < NSTRS; i++) {
		if (!strs[i])
			continue;

		len += strlen(strs[i]->str);
	}

	buf = malloc(len + 1);
	ASSERT(buf);

	buf[0] = '\0';

	for (i = 0; i < NSTRS; i++) {
		if (!strs[i])
			continue;

		strcat(buf, strs[i]->str);

		str_putref(strs[i]);
	}

	out = str_alloc(buf);
	ASSERT(out);

	return out;
}

void str_free(struct str *str)
{
	if (!str)
		return;

	ASSERT3U(str->refcnt, ==, 0);

	free(str->str);
	free(str);
}

struct str *str_getref(struct str *str)
{
	if (!str)
		return NULL;

	ASSERT3U(str->refcnt, >=, 1);

	str->refcnt++;
	return str;
}

void str_putref(struct str *str)
{
	if (!str)
		return;

	ASSERT3S(str->refcnt, >=, 1);

	str->refcnt--;

	if (!str->refcnt)
		str_free(str);
}