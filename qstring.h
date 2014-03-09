#ifndef __QSTRING_H
#define __QSTRING_H

#include <libnvpair.h>

extern void parse_query_string(nvlist_t *vars, const char *qs, size_t len);

#endif
