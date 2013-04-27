#ifndef __LISTING_H
#define __LISTING_H

#include <stdlib.h>

#include "post.h"
#include "mangle.h"

extern char *listing(struct post *post, char *fname);

static inline char *listing_str(char *str)
{
	char *tmp;

	tmp = mangle_htmlescape(str);

	free(str);

	return tmp;
}

#endif
