/*
 * Copyright (c) 2013-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include "error.h"
#include "nvl.h"
#include "vars_impl.h"

static void __init_scope(struct vars *vars)
{
	int ret;

	ret = nvlist_alloc(&vars->scopes[vars->cur], NV_UNIQUE_NAME, 0);
	ASSERT0(ret);
}

static void __free_scope(nvlist_t *scope)
{
	nvlist_free(scope);
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

void vars_set_str(struct vars *vars, const char *name, const char *val)
{
	nvl_set_str(C(vars), name, val);
}

void vars_set_int(struct vars *vars, const char *name, uint64_t val)
{
	nvl_set_int(C(vars), name, val);
}

void vars_set_nvl_array(struct vars *vars, const char *name,
			nvlist_t **val, uint_t nval)
{
	nvl_set_nvl_array(C(vars), name, val, nval);
}

nvpair_t *vars_lookup(struct vars *vars, const char *name)
{
	nvpair_t *ret;
	int scope;

	for (scope = vars->cur; scope >= 0; scope--) {
		ret = nvl_lookup(SCOPE(vars, scope), name);
		if (ret)
			return ret;
	}

	return NULL;
}

char *vars_lookup_str(struct vars *vars, const char *name)
{
	nvpair_t *pair;
	char *out;
	int ret;

	pair = vars_lookup(vars, name);
	ASSERT(pair);

	ASSERT3S(nvpair_type(pair), ==, DATA_TYPE_STRING);

	ret = nvpair_value_string(pair, &out);
	ASSERT0(ret);

	return out;
}

uint64_t vars_lookup_int(struct vars *vars, const char *name)
{
	nvpair_t *pair;
	uint64_t out;
	int ret;

	pair = vars_lookup(vars, name);
	ASSERT(pair);

	ASSERT3S(nvpair_type(pair), ==, DATA_TYPE_STRING);

	ret = nvpair_value_uint64(pair, &out);
	ASSERT0(ret);

	return out;
}

void vars_merge(struct vars *vars, nvlist_t *items)
{
	int ret;

	ret = nvlist_merge(C(vars), items, 0);

	ASSERT0(ret);
}
