#ifndef __ITER_H
#define __ITER_H

#include <stddef.h>
#include <sys/list.h>

#define list_for_each(pos, list) \
	for (pos = list_head(list); pos; pos = list_next((list), pos))

#endif
