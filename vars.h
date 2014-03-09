#ifndef __VARS2_H
#define __VARS2_H

#include <libnvpair.h>

#include "config.h"

struct vars {
	nvlist_t *scopes[VAR_MAX_SCOPES];
	int cur;
};

extern void vars_init(struct vars *vars);
extern void vars_destroy(struct vars *vars);
extern void vars_scope_push(struct vars *vars);
extern void vars_scope_pop(struct vars *vars);
extern void vars_set_str(struct vars *vars, const char *name, char *val);
extern void vars_set_int(struct vars *vars, const char *name, uint64_t val);
extern void vars_set_nvl_array(struct vars *vars, const char *name,
		nvlist_t **val, uint_t nval);
extern nvpair_t *vars_lookup(struct vars *vars, const char *name);
extern char *vars_lookup_str(struct vars *vars, const char *name);
extern uint64_t vars_lookup_int(struct vars *vars, const char *name);
extern void vars_merge(struct vars *vars, nvlist_t *items);
extern char *pair2str(nvpair_t *pair);
extern uint64_t pair2int(nvpair_t *pair);
extern nvlist_t *pair2nvl(nvpair_t *pair);
extern void vars_dump(struct vars *vars);

extern nvlist_t *nvl_alloc();
extern void nvl_set_str(nvlist_t *nvl, const char *name, char *val);
extern void nvl_set_int(nvlist_t *nvl, const char *name, uint64_t val);
extern void nvl_set_str_array(nvlist_t *nvl, const char *name, char **val,
			      uint_t nval);
extern void nvl_set_nvl_array(nvlist_t *nvl, const char *name,
			      nvlist_t **val, uint_t nval);

#endif
