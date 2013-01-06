#ifndef __VARS_H
#define __VARS_H

#include "config.h"
#include "avl.h"

enum var_type {
	VT_STRING,	/* null-terminated string */
	VT_INT,		/* 64-bit uint */
};

struct var_val {
	enum var_type type;
	union {
		char *str;
		uint64_t i;
	};
};

struct var {
	struct avl_node tree;
	char *name;
	struct var_val val;
};

struct vars {
	struct avl_root scopes[VAR_MAX_SCOPES];
	int cur;
};

extern void vars_init(struct vars *vars);

#endif
