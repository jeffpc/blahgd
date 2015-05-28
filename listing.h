#ifndef __LISTING_H
#define __LISTING_H

#include <stdlib.h>

#include "post.h"
#include "mangle.h"
#include "str.h"

extern struct str *listing(struct post *post, char *fname);

static inline struct str *listing_str(struct str *str)
{
	char *tmp;

	tmp = mangle_htmlescape(str->str);

	str_putref(str);

	return str_alloc(tmp);
}

#endif
