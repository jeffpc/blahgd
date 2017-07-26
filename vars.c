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

#include <jeffpc/error.h>

#include "nvl.h"
#include "vars.h"

#define SCOPE(vars, scope)	((vars)->scopes[scope])
#define C(vars)			SCOPE((vars), (vars)->cur)

static void __init_scope(struct vars *vars)
{
	vars->scopes[vars->cur] = nvl_alloc();
	ASSERT(vars->scopes[vars->cur]);
}

static void __free_scope(struct nvlist *scope)
{
	nvl_putref(scope);
}

void vars_init(struct vars *vars)
{
	vars->cur = 0;

	__init_scope(vars);
}

void vars_destroy(struct vars *vars)
{
	int i;

	for (i = 0; i <= vars->cur; i++)
		__free_scope(vars->scopes[i]);
}

void vars_scope_push(struct vars *vars)
{
	vars->cur++;

	ASSERT(vars->cur < VAR_MAX_SCOPES);

	__init_scope(vars);
}

void vars_scope_pop(struct vars *vars)
{
	vars->cur--;

	__free_scope(vars->scopes[vars->cur + 1]);

	if (vars->cur < 0)
		vars_scope_push(vars);

	ASSERT(vars->cur >= 0);
}

#define WRAP_SET1(varfxn, nvlfxn, ctype)				\
void varfxn(struct vars *vars, const char *name, ctype val)		\
{									\
	int ret;							\
									\
	ret = nvlfxn(C(vars), name, val);				\
	ASSERT0(ret);							\
}

#define WRAP_SET2(varfxn, nvlfxn, ctype)				\
void varfxn(struct vars *vars, const char *name, ctype val, size_t len)	\
{									\
	int ret;							\
									\
	ret = nvlfxn(C(vars), name, val, len);				\
	ASSERT0(ret);							\
}

WRAP_SET1(vars_set_str, nvl_set_str, struct str *);
WRAP_SET1(vars_set_int, nvl_set_int, uint64_t);
WRAP_SET2(vars_set_array, nvl_set_array, struct nvval *);

const struct nvpair *vars_lookup(struct vars *vars, const char *name)
{
	const struct nvpair *ret;
	int scope;

	for (scope = vars->cur; scope >= 0; scope--) {
		ret = nvl_lookup(SCOPE(vars, scope), name);
		if (!IS_ERR(ret))
			return ret;
	}

	return NULL;
}

struct str *vars_lookup_str(struct vars *vars, const char *name)
{
	const struct nvpair *pair;
	struct str *ret;

	pair = vars_lookup(vars, name);
	ASSERT(pair);

	ret = nvpair_value_str(pair);
	ASSERT(!IS_ERR(ret));

	return ret;
}

uint64_t vars_lookup_int(struct vars *vars, const char *name)
{
	const struct nvpair *pair;
	uint64_t ret;
	int err;

	pair = vars_lookup(vars, name);
	ASSERT(pair);

	err = nvpair_value_int(pair, &ret);
	ASSERT(!err);

	return ret;
}

void vars_merge(struct vars *vars, struct nvlist *items)
{
	int ret;

	ret = nvl_merge(C(vars), items);
	ASSERT0(ret);
}

void vars_dump(struct vars *vars)
{
	int i;

	for (i = 0; i <= vars->cur; i++) {
		fprintf(stderr, "VARS DUMP scope %d @ %p\n", i,
			vars->scopes[i]);
		nvl_dump_file(stderr, vars->scopes[i]);
	}
}
