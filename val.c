#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <umem.h>

#include "val.h"
#include "error.h"
#include "utils.h"

static umem_cache_t *val_cache;

void init_val_subsys()
{
	val_cache = umem_cache_create("val-cache", sizeof(struct val),
				      0, NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(val_cache);
}

static void __val_init(struct val *val, enum val_type type)
{
	val->type = type;

	switch (type) {
		case VT_INT:
			val->i = 0;
			break;
		case VT_STR:
			val->str = NULL;
			break;
	}
}

static void __val_cleanup(struct val *val)
{
	switch (val->type) {
		case VT_INT:
			break;
		case VT_STR:
			free(val->str);
			break;
	}
}

struct val *val_alloc(enum val_type type)
{
	struct val *val;

	val = umem_cache_alloc(val_cache, 0);
	if (!val)
		return val;

	atomic_set(&val->refcnt, 1);

	__val_init(val, type);

	return val;
}

void val_free(struct val *val)
{
	ASSERT(val);
	ASSERT3U(atomic_read(&val->refcnt), ==, 0);

	__val_cleanup(val);

	umem_cache_free(val_cache, val);
}

#define DEF_VAL_SET(fxn, vttype, valelem, ctype)		\
int val_set_##fxn(struct val *val, ctype v)			\
{								\
	__val_cleanup(val);					\
								\
	val->type = vttype;					\
	val->valelem = v;					\
								\
	return 0;						\
}

DEF_VAL_SET(int, VT_INT, i, uint64_t)
DEF_VAL_SET(str, VT_STR, str, char *)

void val_dump(struct val *val, int indent)
{
	switch (val->type) {
		case VT_STR:
			fprintf(stderr, "%*s'%s'\n", indent, "", val->str);
			break;
		case VT_INT:
			fprintf(stderr, "%*s%lu\n", indent, "", val->i);
			break;
		default:
			fprintf(stderr, "Unknown type %d\n", val->type);
			break;
	}
}
