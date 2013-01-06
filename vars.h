#ifndef __VARS_H
#define __VARS_H

#include "config.h"
#include "avl.h"

enum var_type {
	VT_NIL = 0,	/* invalid */
	VT_STR,		/* null-terminated string */
	VT_INT,		/* 64-bit uint */
	VT_VARS,	/* name-value set of variables */
};

struct var;

struct var_val {
	enum var_type type;
	union {
		struct var *vars[VAR_MAX_VARS_SIZE];
		char *str;
		uint64_t i;
	};
};

struct var {
	struct avl_node tree;
	const char *name;
	struct var_val val[VAR_MAX_ARRAY_SIZE];
};

struct vars {
	struct avl_root scopes[VAR_MAX_SCOPES];
	int cur;
};

extern void vars_init(struct vars *vars);
extern void vars_scope_push(struct vars *vars);
extern void vars_scope_pop(struct vars *vars);
extern void vars_dump(struct vars *vars);

extern struct var *var_alloc(const char *name);
extern void var_free(struct var *v);

extern struct var *var_lookup(struct vars *vars, const char *name);

extern int var_append(struct vars *vars, const char *name, struct var_val *vv);

#endif
