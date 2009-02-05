#ifndef __DIR_H
#define __DIR_H

#include "post.h"

#define SORT_ASC	0x00	/* default */
#define SORT_DESC	0x01
#define SORT_NUMERIC	0x00	/* default */
#define SORT_STRING	0x02

extern int sorted_readdir_loop(DIR *dir, struct post *post,
			       void(*f)(struct post*, char*, void*),
			       void *data, int updown, int limit);

#endif
