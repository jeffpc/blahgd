#ifndef __NVL_H
#define __NVL_H

#include <stdbool.h>
#include <libnvpair.h>

extern nvlist_t *nvl_alloc(void);
extern void nvl_set_str(nvlist_t *nvl, const char *name, const char *val);
extern void nvl_set_int(nvlist_t *nvl, const char *name, uint64_t val);
extern void nvl_set_bool(nvlist_t *nvl, const char *name, bool val);
extern void nvl_set_char(nvlist_t *nvl, const char *name, char val);
extern void nvl_set_nvl(nvlist_t *nvl, const char *name, nvlist_t *val);
extern void nvl_set_str_array(nvlist_t *nvl, const char *name, char **val,
			      uint_t nval);
extern void nvl_set_nvl_array(nvlist_t *nvl, const char *name,
			      nvlist_t **val, uint_t nval);
extern nvpair_t *nvl_lookup(nvlist_t *nvl, const char *name);
extern char *nvl_lookup_str(nvlist_t *nvl, const char *name);
extern uint64_t nvl_lookup_int(nvlist_t *nvl, const char *name);
extern void nvl_dump(nvlist_t *nvl);

extern char *pair2str(nvpair_t *pair);
extern uint64_t pair2int(nvpair_t *pair);
extern bool pair2bool(nvpair_t *pair);
extern char pair2char(nvpair_t *pair);
extern nvlist_t *pair2nvl(nvpair_t *pair);

#endif
