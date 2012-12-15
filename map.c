#include <string.h>
#include <errno.h>

#include "map.h"

#include "ini.h"

int insert_map(struct avl_root *map, const char *name, const char *value)
{
	struct map *m;

	m = malloc(sizeof(*m));
	if (!m)
		return ENOMEM;

	m->key = strdup(name);
	m->value = strdup(value);

	if (!m->key || !m->value)
		goto err;

	if (avl_insert_node(map, &m->node))
		goto err;

	return 0;

err:
	free(m->key);
	free(m->value);
	free(m);

	return ENOMEM;
}

static int __map_parser(void *user, const char *section, const char *name,
			const char *value)
{
	struct avl_root *map = user;

	/* skip everything not in the map section */
	if (strcmp(section, "map"))
		return 1;

	return !insert_map(map, name, value);
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

	init_map(map);

	return ini_parse(path, __map_parser, map);
}

void init_map(struct avl_root *map)
{
	AVL_ROOT_INIT(map, map_cmp, 0);
}

void free_map(struct avl_root *map)
{
	struct avl_node *node;
	struct map *m;

	while (map->root) {
		node = avl_find_minimum(map);
		avl_remove_node(map, node);

		m = container_of(node, struct map, node);
		free(m->key);
		free(m->value);
		free(m);
	}
}
