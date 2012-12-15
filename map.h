#ifndef __MAP_H
#define __MAP_H

#include "avl.h"

struct map {
	struct avl_node node;
	char *key;
	char *value;
};

extern int insert_map(struct avl_root *map, const char *name, const char *value);
extern int load_map(struct avl_root *map, char *fmt);
extern void init_map(struct avl_root *map);
extern void free_map(struct avl_root *map);

#endif
