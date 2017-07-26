/*
 * Copyright (c) 2013-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#ifndef __VARS2_H
#define __VARS2_H

#include "config.h"
#include "nvl.h"

struct vars {
	struct nvlist *scopes[VAR_MAX_SCOPES];
	int cur;
};

extern void vars_init(struct vars *vars);
extern void vars_destroy(struct vars *vars);
extern void vars_scope_push(struct vars *vars);
extern void vars_scope_pop(struct vars *vars);
extern void vars_set_str(struct vars *vars, const char *name, struct str *val);
extern void vars_set_int(struct vars *vars, const char *name, uint64_t val);
extern void vars_set_array(struct vars *vars, const char *name,
		struct nvval *val, size_t nval);
extern const struct nvpair *vars_lookup(struct vars *vars, const char *name);
extern struct str *vars_lookup_str(struct vars *vars, const char *name);
extern uint64_t vars_lookup_int(struct vars *vars, const char *name);
extern void vars_merge(struct vars *vars, struct nvlist *items);
extern void vars_dump(struct vars *vars);

#endif
