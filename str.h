#ifndef __STR_H
#define __STR_H

#include "vars.h"
#include "atomic.h"

/* ref-counted string */

struct str {
	char *str;
	atomic_t refcnt;
};

extern struct str *str_alloc(char *s);
extern size_t str_len(const struct str *str);
extern struct str *str_dup(const char *s);
extern struct str *str_cat5(struct str *a, struct str *b, struct str *c,
			    struct str *d, struct str *e);
extern void str_free(struct str *str);
extern struct str *str_getref(struct str *str);
extern void str_putref(struct str *str);

#define str_cat4(a, b, c, d)		str_cat5((a), (b), (c), (d), NULL)
#define str_cat3(a, b, c)		str_cat5((a), (b), (c), NULL, NULL)
#define str_cat(a, b)			str_cat5((a), (b), NULL, NULL, NULL)

#define STR_DUP(s)			\
	({				\
		struct str *_s;		\
		_s = str_dup(s);	\
		ASSERT(_s);		\
		_s;			\
	})

#endif
