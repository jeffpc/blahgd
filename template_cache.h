#ifndef __TEMPLATE_CACHE_H
#define __TEMPLATE_CACHE_H

extern void init_template_cache(void);
extern struct str *template_cache_get(const char *name);

#endif
