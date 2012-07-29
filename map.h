#ifndef __MAP_H
#define __MAP_H

#include "avl.h"

struct map {
	struct avl_node node;
	char *key;
	char *value;
};

extern int load_map(struct avl_root *map, char *fmt);

#endif
