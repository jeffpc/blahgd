#ifndef __VARS_H
#define __VARS_H

#include <sys/avl.h>

#include "config.h"
#include "utils.h"

enum val_type {
	VT_INT = 0,	/* 64-bit uint */
	VT_STR,		/* null-terminated string */
	VT_NV,		/* name-value set of values */
	VT_LIST,	/* array of values */
};

struct val;

struct val_item {
	avl_node_t tree;
	union {
		uint64_t i;
		char name[VAR_VAL_KEY_MAXLEN];
	} key;
	struct val *val;
};

struct val {
	enum val_type type;
	int refcnt;
	union {
		uint64_t i;
		char *str;
		avl_tree_t tree;
	};
};

struct var {
	avl_node_t tree;
	char name[VAR_NAME_MAXLEN];
	struct val *val;
};

struct vars {
	avl_tree_t scopes[VAR_MAX_SCOPES];
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
extern int var_set(struct vars *vars, const char *name, struct val *val);

extern struct val *val_alloc(enum val_type type);
extern void val_free(struct val *v);
extern int val_set_int(struct val *val, uint64_t v);
extern int val_set_str(struct val *val, char *v);
extern int val_set_list(struct val *val, uint64_t idx, struct val *sub);
extern int val_set_nv(struct val *val, char *name, struct val *sub);
extern int val_set_nvint(struct val *val, char *name, uint64_t v);
extern int val_set_nvstr(struct val *val, char *name, char *v);
extern void val_dump(struct val *v, int indent);

#define valcat(a, b)		valcat5((a), (b), NULL, NULL, NULL)
#define valcat3(a, b, c)	valcat5((a), (b), (c), NULL, NULL)
#define valcat4(a, b, c, d)	valcat5((a), (b), (c), (d), NULL)
extern struct val *valcat5(struct val *a, struct val *b, struct val *c,
			   struct val *d, struct val *e);

static inline struct val *val_getref(struct val *vv)
{
	if (!vv)
		return NULL;

	ASSERT3U(vv->refcnt, >=, 1);

	vv->refcnt++;
	return vv;
}

static inline void val_putref(struct val *vv)
{
	if (!vv)
		return;

	ASSERT3S(vv->refcnt, >=, 1);

	vv->refcnt--;

	if (!vv->refcnt)
		val_free(vv);
}

#define VAR_LOOKUP_VAL(vars, n)			\
	({					\
		struct var *_x;			\
		_x = var_lookup((vars), (n));	\
		ASSERT(_x);			\
		val_getref(_x->val);		\
	})
#define VAR_SET(vars, n, val)		ASSERT0(var_set((vars), (n), (val)))

#define VAR_SET_STR(vars, n, v)			\
	do {					\
		struct val *_x;			\
		_x = VAL_ALLOC_STR(v);		\
		VAR_SET((vars), (n), _x);	\
	} while (0)

#define VAR_SET_INT(vars, n, v)			\
	do {					\
		struct val *_x;			\
		_x = VAL_ALLOC_INT(v);		\
		VAR_SET((vars), (n), _x);	\
	} while (0)

#define VAL_ALLOC(t)				\
	({					\
		struct val *_x;			\
		_x = val_alloc(t);		\
		ASSERT(_x);			\
		_x;				\
	})

#define VAL_ALLOC_STR(v)			\
	({					\
		struct val *_x;			\
		_x = VAL_ALLOC(VT_STR);		\
		VAL_SET_STR(_x, v);		\
		_x;				\
	})

#define VAL_ALLOC_INT(v)			\
	({					\
		struct val *_x;			\
		_x = VAL_ALLOC(VT_INT);		\
		VAL_SET_INT(_x, (v));		\
		_x;				\
	})

#define VAL_SET_INT(val, v)		ASSERT0(val_set_int((val), (v)))
#define VAL_SET_STR(val, v)		ASSERT0(val_set_str((val), (v)))
#define VAL_SET_NVINT(val, n, v)	ASSERT0(val_set_nvint((val), (n), (v)))
#define VAL_SET_NVSTR(val, n, v)	ASSERT0(val_set_nvstr((val), (n), (v)))
#define VAL_SET_NV(val, n, v)		ASSERT0(val_set_nv((val), (n), (v)))
#define VAL_SET_LIST(val, idx, v)	ASSERT0(val_set_list((val), (idx), (v)))

#define VAL_DUP_STR(v)		VAL_ALLOC_STR(xstrdup(v))

#endif
