#ifndef __VARS_H
#define __VARS_H

#include "config.h"
#include "avl.h"
#include "utils.h"

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
	char name[VAR_MAX_VAR_NAME];
	struct var_val val[VAR_MAX_ARRAY_SIZE];
};

struct vars {
	struct avl_root scopes[VAR_MAX_SCOPES];
	int cur;
};

extern void init_var_subsys();

extern void vars_init(struct vars *vars);
extern void vars_destroy(struct vars *vars);
extern void vars_scope_push(struct vars *vars);
extern void vars_scope_pop(struct vars *vars);
extern void vars_dump(struct vars *vars);

extern struct var *var_alloc(const char *name);
extern void var_free(struct var *v);
extern void var_dump(struct var *v, int indent);
extern struct var *var_lookup(struct vars *vars, const char *name);
extern int var_append(struct vars *vars, const char *name, struct var_val *vv);

extern struct var_val *var_val_alloc();
extern void var_val_free(struct var_val *vv);
extern void var_val_dump(struct var_val *vv, int idx, int indent);

static inline struct var *var_alloc_int(const char *name, uint64_t val)
{
	struct var *v;

	v = var_alloc(name);
	if (!v)
		return NULL;

	v->val[0].type = VT_INT;
	v->val[0].i    = val;

	return v;
}

static inline struct var *var_alloc_str(const char *name, const char *val)
{
	struct var *v;

	v = var_alloc(name);
	if (!v)
		return NULL;

	v->val[0].type = VT_STR;
	v->val[0].str  = xstrdup(val);

	if (!v->val[0].str) {
		var_free(v);
		return NULL;
	}

	return v;
}

#define __VAR_ALLOC(n, v, t)			\
	({					\
		struct var *_x;			\
		_x = var_alloc_##t((n), (v));	\
		ASSERT(_x);			\
		_x;				\
	})

#define VAR_ALLOC_INT(n, v)	__VAR_ALLOC((n), (v), int)
#define VAR_ALLOC_STR(n, v)	__VAR_ALLOC((n), (v), str)

static inline struct var_val *var_val_alloc_str(char *str)
{
	struct var_val *vv;

	vv = var_val_alloc();
	if (!vv)
		return NULL;

	vv->type = VT_STR;
	vv->str = str;

	return vv;
}

#define __VAR_VAL_ALLOC(v, t)			\
	({					\
		struct var_val *_x;		\
		_x = var_val_alloc_##t(v);	\
		ASSERT(_x);			\
		_x;				\
	})

#define VAR_VAL_ALLOC_STR(v)	__VAR_VAL_ALLOC((v), str)

#endif
