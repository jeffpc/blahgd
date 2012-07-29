#include <string.h>

#include "map.h"

#include "ini.h"

static int __map_parser(void *user, const char *section, const char *name,
			const char *value)
{
	struct avl_root *map = user;
	struct map *m;

	/* skip everything not in the map section */
	if (strcmp(section, "map"))
		return 1;

	m = malloc(sizeof(*m));
	if (!m)
		return 0;

	m->key = strdup(name);
	m->value = strdup(value);

	if (!m->key || !m->value)
		goto err;

	if (avl_insert_node(map, &m->node))
		goto err;

	return 1;

err:
	free(m->key);
	free(m->value);
	free(m);
	return 0;
}

static int map_cmp(struct avl_node *n1, struct avl_node *n2)
{
	struct map *a = container_of(n1, struct map, node);
	struct map *b = container_of(n2, struct map, node);

	return strcmp(a->key, b->key);
}

int load_map(struct avl_root *map, char *fmt)
{
	char path[FILENAME_MAX];

	snprintf(path, sizeof(path), "templates/%s/map", fmt);

	AVL_ROOT_INIT(map, map_cmp, 0);

	return ini_parse(path, __map_parser, map);
}
