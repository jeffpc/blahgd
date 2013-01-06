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
