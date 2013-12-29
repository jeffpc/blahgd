#ifndef __LISTING_H
#define __LISTING_H

#include <stdlib.h>

#include "post.h"
#include "mangle.h"
#include "vars.h"

extern struct val *listing(struct post *post, char *fname);

static inline struct val *listing_str(struct val *val)
{
	char *tmp;

	ASSERT3U(val->type, ==, VT_STR);

	tmp = mangle_htmlescape(val->str);

	val_putref(val);

	return VAL_ALLOC_STR(tmp);
}

#endif
