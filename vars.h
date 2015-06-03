#ifndef __VARS2_H
#define __VARS2_H

#include "config.h"
#include "nvl.h"

struct vars {
	nvlist_t *scopes[VAR_MAX_SCOPES];
	int cur;
};

extern void vars_init(struct vars *vars);
extern void vars_destroy(struct vars *vars);
extern void vars_scope_push(struct vars *vars);
extern void vars_scope_pop(struct vars *vars);
extern void vars_set_str(struct vars *vars, const char *name, const char *val);
extern void vars_set_int(struct vars *vars, const char *name, uint64_t val);
extern void vars_set_nvl_array(struct vars *vars, const char *name,
		nvlist_t **val, uint_t nval);
extern nvpair_t *vars_lookup(struct vars *vars, const char *name);
extern char *vars_lookup_str(struct vars *vars, const char *name);
extern uint64_t vars_lookup_int(struct vars *vars, const char *name);
extern void vars_merge(struct vars *vars, nvlist_t *items);
extern void vars_dump(struct vars *vars);

#endif
