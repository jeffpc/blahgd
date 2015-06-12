/*
 * Copyright (c) 2014-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
