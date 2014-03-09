#ifndef __QSTRING_H
#define __QSTRING_H

#include <libnvpair.h>
#include <string.h>

extern void parse_query_string_len(nvlist_t *vars, const char *qs, size_t len);

static inline void parse_query_string(nvlist_t *vars, const char *qs)
{
	size_t len;

	if (qs)
		len = strlen(qs);
	else
		len = 0;

	parse_query_string_len(vars, qs, len);
}

#endif
