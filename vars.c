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
static umem_cache_t *var_val_cache;

void init_var_subsys()
{
	var_cache = umem_cache_create("var-cache", sizeof(struct var), 0,
				      NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(var_cache);

	var_val_cache = umem_cache_create("var-val-cache", sizeof(struct var_val),
					  0, NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(var_val_cache);
}

static int cmp(const void *va, const void *vb)
{
	const struct var *a = va;
	const struct var *b = vb;
	int ret;

	ret = strncmp(a->name, b->name, VAR_MAX_VAR_NAME);

	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

static void __init_scope(struct vars *vars)
{
	avl_create(&vars->scopes[vars->cur], cmp, sizeof(struct var),
		   offsetof(struct var, tree));
}

static void __free_scope(avl_tree_t *tree)
{
	struct var *v;
	void *cookie;

	cookie = NULL;
	while ((v = avl_destroy_nodes(tree, &cookie)))
		var_putref(v);

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

	strncpy(key.name, name, VAR_MAX_VAR_NAME);

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

	ASSERT(strlen(name) < VAR_MAX_VAR_NAME);

	v = umem_cache_alloc(var_cache, 0);
	if (!v)
		return NULL;

	memset(v, 0, sizeof(struct var));
	strncpy(v->name, name, VAR_MAX_VAR_NAME);

	v->refcnt = 1;

	return v;
}

void var_free(struct var *v)
{
	int i, j;

	ASSERT(v);
	ASSERT3U(v->refcnt, ==, 0);

	for (i = 0; i < VAR_MAX_ARRAY_SIZE; i++) {
		switch (v->val[i].type) {
			case VT_NIL:
			case VT_INT:
				break;
			case VT_STR:
				break;
			case VT_VARS:
				for (j = 0; j < VAR_MAX_VARS_SIZE; j++)
					var_putref(v->val[i].vars[j]);
				break;
		}
	}

	umem_cache_free(var_cache, v);
}

struct var_val *var_val_alloc()
{
	return umem_cache_alloc(var_val_cache, 0);
}

void var_val_free(struct var_val *vv)
{
	umem_cache_free(var_val_cache, vv);
}

int var_append(struct vars *vars, const char *name, struct var_val *vv)
{
	struct var key;
	struct var *v;
	bool shouldfree;
	int i, j;

	shouldfree = false;
	strncpy(key.name, name, VAR_MAX_VAR_NAME);

	v = avl_find(&vars->scopes[vars->cur], &key, NULL);
	if (!v) {
		shouldfree = true;

		v = var_alloc(name);
		if (!v)
			return ENOMEM;

		avl_add(&vars->scopes[vars->cur], &v->tree);
	}

	for (i = 0; i < VAR_MAX_ARRAY_SIZE; i++) {
		if (v->val[i].type != VT_NIL)
			continue;

		if (vv->type == VT_VARS)
			for (j = 0; j < VAR_MAX_VARS_SIZE; j++)
				var_getref(vv->vars[j]);

		v->val[i] = *vv;

		return 0;
	}

	if (shouldfree)
		var_putref(v);

	return E2BIG;
}

void var_val_dump(struct var_val *vv, int idx, int indent)
{
	int i;

	fprintf(stderr, "%*s   [%02d] ", indent, "", idx);

	switch (vv->type) {
		case VT_NIL:
			fprintf(stderr, "NIL\n");
			break;
		case VT_STR:
			fprintf(stderr, "'%s'\n", vv->str);
			break;
		case VT_INT:
			fprintf(stderr, "%lu\n", vv->i);
			break;
		case VT_VARS:
			fprintf(stderr, "VARS\n");
			for (i = 0; i < VAR_MAX_VARS_SIZE; i++)
				if (vv->vars[i])
					var_dump(vv->vars[i], indent + 6);
			break;
		default:
			fprintf(stderr, "Unknown type %d\n", vv->type);
			break;
	}
}

void var_dump(struct var *v, int indent)
{
	int i;

	fprintf(stderr, "%*s-> '%s'\n", indent, "", v->name);

	for (i = 0; i < VAR_MAX_ARRAY_SIZE; i++)
		if (v->val[i].type != VT_NIL)
			var_val_dump(&v->val[i], i, indent);
}

void vars_dump(struct vars *vars)
{
	struct var *v;
	int scope;

	fprintf(stderr, "%p: VARS DUMP BEGIN\n", vars);

	for (scope = vars->cur; scope >= 0; scope--) {
		avl_tree_t *s = &vars->scopes[scope];

		fprintf(stderr, "%p: scope %2d:\n", vars, scope);

		for (v = avl_first(s); v; v = AVL_NEXT(s, v))
			var_dump(v, 0);
	}

	fprintf(stderr, "%p: VARS DUMP END\n", vars);
}
