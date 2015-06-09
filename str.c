#include <stdlib.h>
#include <stdbool.h>
#include <umem.h>

#include "str.h"
#include "val.h"
#include "utils.h"

static umem_cache_t *str_cache;

void init_str_subsys(void)
{
	str_cache = umem_cache_create("str-cache", sizeof(struct str),
				      0, NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(str_cache);
}

static struct str *__alloc(char *s, bool copy)
{
	struct str *str;

	str = umem_cache_alloc(str_cache, 0);
	if (!str)
		return NULL;

	atomic_set(&str->refcnt, 1);

	if (copy) {
		strcpy(str->inline_str, s);
		str->str = str->inline_str;
	} else {
		str->str = s;
	}

	return str;
}

struct str *str_dup(const char *s)
{
	if (!s)
		return __alloc("", true);

	if (strlen(s) <= STR_INLINE_LEN)
		return __alloc((char *) s, true);

	return str_alloc(strdup(s));
}

struct str *str_alloc(char *s)
{
	return __alloc(s, false);
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

	if (str->str != str->inline_str)
		free(str->str);
	umem_cache_free(str_cache, str);
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
