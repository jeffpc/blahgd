#ifndef __ATOMIC_H
#define __ATOMIC_H

#include <stdint.h>

typedef struct {
	volatile uint64_t v;
} atomic64_t;

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
#define atomic_add(var, val)	__sync_add_and_fetch(&(var)->v, (val))
#define atomic_sub(var, val)	__sync_sub_and_fetch(&(var)->v, (val))
#define atomic_inc(var)		atomic_add((var), 1)
#define atomic_dec(var)		atomic_sub((var), 1)

#endif
