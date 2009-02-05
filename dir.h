#ifndef __DIR_H
#define __DIR_H

#include "post.h"

#define SORT_ASC	0
#define SORT_DESC	1

extern int sorted_readdir_loop(DIR *dir, struct post *post,
			       void(*f)(struct post*, char*, void*),
			       void *data, int updown, int limit);

#endif
