#ifndef __VAL_H
#define __VAL_H

#include "config.h"
#include "utils.h"
#include "atomic.h"

enum val_type {
	VT_INT = 0,	/* 64-bit uint */
	VT_STR,		/* null-terminated string */
};

struct val {
	enum val_type type;
	atomic_t refcnt;
	union {
		uint64_t i;
		char *str;
	};
};

extern void init_val_subsys(void);

extern struct val *val_alloc(enum val_type type);
extern void val_free(struct val *v);
extern int val_set_int(struct val *val, uint64_t v);
extern int val_set_str(struct val *val, char *v);
extern void val_dump(struct val *v, int indent);

static inline struct val *val_getref(struct val *vv)
{
	if (!vv)
		return NULL;

	ASSERT3U(atomic_read(&vv->refcnt), >=, 1);

	atomic_inc(&vv->refcnt);

	return vv;
}

static inline void val_putref(struct val *vv)
{
	if (!vv)
		return;

	ASSERT3S(atomic_read(&vv->refcnt), >=, 1);

	if (!atomic_dec(&vv->refcnt))
		val_free(vv);
}

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

#define VAL_DUP_STR(v)		VAL_ALLOC_STR(xstrdup(v))

#endif
