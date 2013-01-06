#include <string.h>

#include "vars.h"

static int cmp(struct avl_node *aa, struct avl_node *ab)
{
	struct var *a = container_of(aa, struct var, tree);
	struct var *b = container_of(ab, struct var, tree);

	return strcmp(a->name, b->name);
}

void vars_init(struct vars *vars)
{
	AVL_ROOT_INIT(&vars->scopes[0], cmp, 0);
	vars->cur = 0;
}

bool is_var(struct vars *vars, const char *name)
{
	struct var key = {
		.name = name,
	};
	struct avl_node *node;
	int scope;

	for (scope = vars->cur; scope >= 0; scope--) {
		node = avl_find_node(&vars->scopes[scope], &key.tree);
		if (node)
			return true;
	}

	return false;
}
