#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <umem.h>

#include "vars.h"
#include "error.h"
#include "utils.h"

static umem_cache_t *var_cache;
static umem_cache_t *val_cache;
static umem_cache_t *val_item_cache;

void init_var_subsys()
{
	var_cache = umem_cache_create("var-cache", sizeof(struct var), 0,
				      NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(var_cache);

	val_cache = umem_cache_create("val-cache", sizeof(struct val),
				      0, NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(val_cache);

	val_item_cache = umem_cache_create("val-item-cache",
					   sizeof(struct val_item), 0, NULL,
					   NULL, NULL, NULL, NULL, 0);
	ASSERT(val_item_cache);
}

static int cmp_var(const void *va, const void *vb)
{
	const struct var *a = va;
	const struct var *b = vb;
	int ret;

	ret = strncmp(a->name, b->name, VAR_NAME_MAXLEN);

	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

static int cmp_val_list(const void *va, const void *vb)
{
	const struct val_item *a = va;
	const struct val_item *b = vb;

	if (a->key.i < b->key.i)
		return -1;
	return (a->key.i != b->key.i);
}

static int cmp_val_nv(const void *va, const void *vb)
{
	const struct val_item *a = va;
	const struct val_item *b = vb;
	int ret;

	ret = strncmp(a->key.name, b->key.name, VAR_VAL_KEY_MAXLEN);

	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

static void __init_scope(struct vars *vars)
{
	avl_create(&vars->scopes[vars->cur], cmp_var, sizeof(struct var),
		   offsetof(struct var, tree));
}

static void __free_scope(avl_tree_t *tree)
{
	struct var *v;
	void *cookie;

	cookie = NULL;
	while ((v = avl_destroy_nodes(tree, &cookie)))
		var_free(v);

	avl_destroy(tree);
}

void vars_init(struct vars *vars)
{
	vars->cur = 0;

	__init_scope(vars);
}

void vars_destroy(struct vars *vars)
{
	int i;

	for (i = 0; i <= vars->cur; i++)
		__free_scope(&vars->scopes[i]);
}

void vars_scope_push(struct vars *vars)
{
	vars->cur++;

	ASSERT(vars->cur < VAR_MAX_SCOPES);

	__init_scope(vars);
}

void vars_scope_pop(struct vars *vars)
{
	vars->cur--;

	__free_scope(&vars->scopes[vars->cur + 1]);

	if (vars->cur < 0)
		vars_scope_push(vars);

	ASSERT(vars->cur >= 0);
}

struct var *var_lookup(struct vars *vars, const char *name)
{
	struct var key;
	struct var *v;
	int scope;

	strncpy(key.name, name, VAR_NAME_MAXLEN);

	for (scope = vars->cur; scope >= 0; scope--) {
		v = avl_find(&vars->scopes[scope], &key, NULL);
		if (v)
			return v;
	}

	return NULL;
}

struct var *var_alloc(const char *name)
{
	struct var *v;

	ASSERT(strlen(name) < VAR_NAME_MAXLEN);

	v = umem_cache_alloc(var_cache, 0);
	if (!v)
		return NULL;

	memset(v, 0, sizeof(struct var));
	strncpy(v->name, name, VAR_NAME_MAXLEN);

	return v;
}

void var_free(struct var *var)
{
	val_putref(var->val);

	umem_cache_free(var_cache, var);
}

static void __val_init(struct val *val, enum val_type type)
{
	val->type = type;

	switch (type) {
		case VT_LIST:
			avl_create(&val->tree, cmp_val_list,
				   sizeof(struct val_item),
				   offsetof(struct val_item, tree));
			break;
		case VT_NV:
			avl_create(&val->tree, cmp_val_nv,
				   sizeof(struct val_item),
				   offsetof(struct val_item, tree));
			break;
		case VT_INT:
			val->i = 0;
			break;
		case VT_STR:
			val->str = NULL;
			break;
		default:
			ASSERT(0);
	}
}

static void __val_cleanup(struct val *val)
{
	struct val_item *vi;
	void *cookie;

	switch (val->type) {
		case VT_INT:
			break;
		case VT_STR:
			free(val->str);
			break;
		case VT_NV:
		case VT_LIST:
			cookie = NULL;
			while ((vi = avl_destroy_nodes(&val->tree, &cookie))) {
				val_putref(vi->val);

				umem_cache_free(val_item_cache, vi);
			}

			avl_destroy(&val->tree);

			break;
		default:
			ASSERT(0);
	}
}

struct val *val_alloc(enum val_type type)
{
	struct val *val;

	val = umem_cache_alloc(val_cache, 0);
	if (!val)
		return val;

	val->refcnt = 1;

	__val_init(val, type);

	return val;
}

void val_free(struct val *val)
{
	ASSERT(val);
	ASSERT3U(val->refcnt, ==, 0);

	__val_cleanup(val);

	umem_cache_free(val_cache, val);
}

static int __val_set_val(struct val *val, enum val_type type, char *name,
			 uint64_t idx, struct val *sub)
{
	struct val_item *vi;

	if ((type != VT_NV) && (type != VT_LIST))
		return EINVAL;

	vi = umem_cache_alloc(val_item_cache, 0);
	if (!vi)
		return ENOMEM;

	vi->val = sub;

	if (type == VT_NV)
		strncpy(vi->key.name, name, VAR_VAL_KEY_MAXLEN);
	else if (type == VT_LIST)
		vi->key.i = idx;

	if (val->type != type) {
		__val_cleanup(val);
		__val_init(val, type);
	}

	avl_add(&val->tree, vi);

	return 0;
}

int val_set_list(struct val *val, uint64_t idx, struct val *sub)
{
	return __val_set_val(val, VT_LIST, NULL, idx, sub);
}

int val_set_nv(struct val *val, char *name, struct val *sub)
{
	if (!name)
		return EINVAL;

	return __val_set_val(val, VT_NV, name, 0, sub);
}

#define DEF_VAL_SET(fxn, vttype, valelem, ctype)		\
int val_set_nv##fxn(struct val *val, char *name, ctype v)	\
{								\
	struct val *sub;					\
	int ret;						\
								\
	sub = val_alloc(vttype);				\
	if (!sub)						\
		return ENOMEM;					\
								\
	sub->valelem = v;					\
								\
	ret = val_set_nv(val, name, sub);			\
	if (ret)						\
		val_putref(sub);				\
								\
	return ret;						\
}								\
								\
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

int var_set(struct vars *vars, const char *name, struct val *val)
{
	struct var key;
	struct var *v;

	strncpy(key.name, name, VAR_NAME_MAXLEN);

	v = avl_find(&vars->scopes[vars->cur], &key, NULL);
	if (!v) {
		v = var_alloc(name);
		if (!v)
			return ENOMEM;

		avl_add(&vars->scopes[vars->cur], v);
	}

	val_putref(v->val);

	v->val = val;

	return 0;
}

static void __dump_val(struct val_item *vi, int indent, enum val_type type)
{
	if (type == VT_LIST)
		fprintf(stderr, "%*s [%lu] = ", indent, "", vi->key.i);
	else
		fprintf(stderr, "%*s [%s] = ", indent, "", vi->key.name);

	val_dump(vi->val, indent + 4);
}

void val_dump(struct val *val, int indent)
{
	struct val_item *vi;

	switch (val->type) {
		case VT_STR:
			fprintf(stderr, "%*s'%s'\n", indent, "", val->str);
			break;
		case VT_INT:
			fprintf(stderr, "%*s%lu\n", indent, "", val->i);
			break;
		case VT_LIST:
		case VT_NV:
			fprintf(stderr, "\n");

			for (vi = avl_first(&val->tree); vi;
			     vi = AVL_NEXT(&val->tree, vi))
				__dump_val(vi, indent + 4, val->type);

			break;
		default:
			fprintf(stderr, "Unknown type %d\n", val->type);
			break;
	}
}

void vars_dump(struct vars *vars)
{
	struct var *v;
	int scope;

	fprintf(stderr, "%p: VARS DUMP BEGIN\n", vars);

	for (scope = vars->cur; scope >= 0; scope--) {
		avl_tree_t *s = &vars->scopes[scope];

		fprintf(stderr, "%p: scope %2d:\n", vars, scope);

		for (v = avl_first(s); v; v = AVL_NEXT(s, v)) {
			fprintf(stderr, "-> '%s'\n", v->name);
			val_dump(v->val, 4);
		}
	}

	fprintf(stderr, "%p: VARS DUMP END\n", vars);
}

struct val *valcat5(struct val *a, struct val *b, struct val *c,
		    struct val *d, struct val *e)
{
#define NVALS	5
	struct val *vals[NVALS] = {a, b, c, d, e};
	size_t len;
	char *buf;
	int i;

	len = 0;

	for (i = 0; i < NVALS; i++) {
		if (!vals[i])
			continue;

		ASSERT3U(vals[i]->type, ==, VT_STR);

		len += strlen(vals[i]->str);
	}

	buf = malloc(len + 1);
	ASSERT(buf);

	buf[0] = '\0';

	for (i = 0; i < NVALS; i++) {
		if (!vals[i])
			continue;

		strcat(buf, vals[i]->str);

		val_putref(vals[i]);
	}

	return VAL_ALLOC_STR(buf);
}
