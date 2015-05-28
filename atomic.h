#ifndef __ATOMIC_H
#define __ATOMIC_H

#include <stdint.h>
#include <atomic.h>

typedef struct {
	volatile uint32_t v;
} atomic_t;

/*
 * The following implement generic set/read/add/sub/inc/dec operations.
 *
 * WARNING: atomic_set and atomic_read may break on some really quirky
 * architectures.
 */

#define atomic_set(var, val)	((var)->v = (val))
#define atomic_read(var)	((var)->v)
#define atomic_add(var, val)	atomic_add_32_nv(&(var)->v, (val))
#define atomic_sub(var, val)	anomic_add_32_nv(&(var)->v, -(val))
#define atomic_inc(var)		atomic_inc_32_nv(&(var)->v)
#define atomic_dec(var)		atomic_dec_32_nv(&(var)->v)

#endif
