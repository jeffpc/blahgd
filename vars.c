#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "vars.h"
#include "error.h"
#include "utils.h"

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

void vars_init(struct vars *vars)
{
	vars->cur = 0;

	__init_scope(vars);
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

	ASSERT(vars->cur >= 0);

	// FIXME: just leaked memory
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

	v = malloc(sizeof(struct var));
	if (!v)
		return NULL;

	memset(v, 0, sizeof(struct var));

	v->name = xstrdup(name);
	if (!v->name) {
		free(v);
		return NULL;
	}

	return v;
}

void var_free(struct var *v)
{
	if (!v)
		return;

	free((void*) v->name);
	free(v);
}

int var_append(struct vars *vars, const char *name, struct var_val *vv)
{
	struct var key = {
		.name = name,
	};
	struct avl_node *node;
	struct var *v;
	int i;

	node = avl_find_node(&vars->scopes[vars->cur], &key.tree);
	if (!node) {
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
