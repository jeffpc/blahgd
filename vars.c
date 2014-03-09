#include "error.h"
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
		ret = nvl_lookup(C(vars), name);
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

nvlist_t *nvl_alloc()
{
	nvlist_t *out;
	int ret;

	ret = nvlist_alloc(&out, NV_UNIQUE_NAME, 0);

	ASSERT0(ret);

	return out;
}

void nvl_set_str(nvlist_t *nvl, const char *name, const char *val)
{
	int ret;

	ret = nvlist_add_string(nvl, name, val);

	ASSERT0(ret);
}

void nvl_set_int(nvlist_t *nvl, const char *name, uint64_t val)
{
	int ret;

	ret = nvlist_add_uint64(nvl, name, val);

	ASSERT0(ret);
}

void nvl_set_str_array(nvlist_t *nvl, const char *name, char **val,
		       uint_t nval)
{
	int ret;

	ret = nvlist_add_string_array(nvl, name, val, nval);

	ASSERT0(ret);
}

void nvl_set_nvl_array(nvlist_t *nvl, const char *name, nvlist_t **val,
		       uint_t nval)
{
	int ret;

	ret = nvlist_add_nvlist_array(nvl, name, val, nval);

	ASSERT0(ret);
}

nvpair_t *nvl_lookup(nvlist_t *nvl, const char *name)
{
	nvpair_t *pair;
	int ret;

	ret = nvlist_lookup_nvpair(nvl, name, &pair);
	if (!ret)
		return pair;

	return NULL;
}

char *nvl_lookup_str(nvlist_t *nvl, const char *name)
{
	nvpair_t *pair;

	pair = nvl_lookup(nvl, name);
	ASSERT(pair);

	ASSERT3U(nvpair_type(pair), ==, DATA_TYPE_STRING);

	return pair2str(pair);
}

uint64_t nvl_lookup_int(nvlist_t *nvl, const char *name)
{
	nvpair_t *pair;

	pair = nvl_lookup(nvl, name);
	ASSERT(pair);

	ASSERT3U(nvpair_type(pair), ==, DATA_TYPE_UINT64);

	return pair2int(pair);
}
