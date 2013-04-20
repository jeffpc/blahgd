#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <umem.h>

#include "vars.h"
#include "error.h"
#include "utils.h"

static umem_cache_t *var_cache;

void init_var_subsys()
{
	var_cache = umem_cache_create("var-cache", sizeof(struct var), 0,
				      NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(var_cache);
}

static int cmp(struct avl_node *aa, struct avl_node *ab)
{
	struct var *a = container_of(aa, struct var, tree);
	struct var *b = container_of(ab, struct var, tree);

	return strcmp(a->name, b->name);
}

static void __init_scope(struct vars *vars)
{
	AVL_ROOT_INIT(&vars->scopes[vars->cur], cmp, 0);
}

static void __free_scope(struct avl_root *root)
{
	struct avl_node *node;

	while ((node = avl_find_minimum(root))) {
		struct var *v = container_of(node, struct var, tree);

		avl_remove_node(root, node);

		var_free(v);
	}
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
	struct var key = {
		.name = name,
	};
	struct avl_node *node;
	int scope;

	for (scope = vars->cur; scope >= 0; scope--) {
		node = avl_find_node(&vars->scopes[scope], &key.tree);
		if (node)
			return container_of(node, struct var, tree);
	}

	return NULL;
}

struct var *var_alloc(const char *name)
{
	struct var *v;

	v = umem_cache_alloc(var_cache, 0);
	if (!v)
		return NULL;

	memset(v, 0, sizeof(struct var));

	v->name = xstrdup(name);
	if (!v->name) {
		umem_cache_free(var_cache, v);
		return NULL;
	}

	return v;
}

void var_free(struct var *v)
{
	if (!v)
		return;

	free((void*) v->name);
	umem_cache_free(var_cache, v);
}

int var_append(struct vars *vars, const char *name, struct var_val *vv)
{
	struct var key = {
		.name = name,
	};
	struct avl_node *node;
	struct var *v;
	bool shouldfree;
	int i;

	shouldfree = false;

	node = avl_find_node(&vars->scopes[vars->cur], &key.tree);
	if (!node) {
		shouldfree = true;

		v = var_alloc(name);
		if (!v)
			return ENOMEM;

		if (avl_insert_node(&vars->scopes[vars->cur], &v->tree)) {
			var_free(v);
			return EEXIST;
		}

		node = &v->tree;
	}

	v = container_of(node, struct var, tree);

	for (i = 0; i < VAR_MAX_ARRAY_SIZE; i++) {
		if (v->val[i].type != VT_NIL)
			continue;

		v->val[i] = *vv;

		return 0;
	}

	if (shouldfree)
		var_free(v);

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
			fprintf(stderr, "%llu\n", vv->i);
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

static int __vars_dump(struct avl_node *node, void *data)
{
	struct var *v = container_of(node, struct var, tree);

	var_dump(v, 0);

	return 0;
}

void vars_dump(struct vars *vars)
{
	int scope;

	fprintf(stderr, "%p: VARS DUMP BEGIN\n", vars);

	for (scope = vars->cur; scope >= 0; scope--) {
		fprintf(stderr, "%p: scope %2d:\n", vars, scope);
		avl_for_each(&vars->scopes[scope], __vars_dump, NULL, NULL, NULL);
	}

	fprintf(stderr, "%p: VARS DUMP END\n", vars);
}
