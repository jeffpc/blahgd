#include "error.h"
#include "nvl.h"

nvlist_t *nvl_alloc(void)
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

	if (!val)
		return;

	ret = nvlist_add_string(nvl, name, val);

	ASSERT0(ret);
}

void nvl_set_int(nvlist_t *nvl, const char *name, uint64_t val)
{
	int ret;

	ret = nvlist_add_uint64(nvl, name, val);

	ASSERT0(ret);
}

void nvl_set_bool(nvlist_t *nvl, const char *name, bool val)
{
	int ret;

	ret = nvlist_add_boolean_value(nvl, name, val);

	ASSERT0(ret);
}

void nvl_set_char(nvlist_t *nvl, const char *name, char val)
{
	int ret;

	ret = nvlist_add_uint8(nvl, name, (uint8_t) val);

	ASSERT0(ret);
}

void nvl_set_nvl(nvlist_t *nvl, const char *name, nvlist_t *val)
{
	int ret;

	ret = nvlist_add_nvlist(nvl, name, val);

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
	if (!pair)
		return NULL;

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

#define DECL_PAIR2(name, nvfxn, rettype, localtype)			\
rettype name(nvpair_t *pair)						\
{									\
	localtype out;							\
	int ret;							\
									\
	ret = nvfxn(pair, &out);					\
	ASSERT0(ret);							\
									\
	return out;							\
}

DECL_PAIR2(pair2str,  nvpair_value_string,        char *,     char *)
DECL_PAIR2(pair2bool, nvpair_value_boolean_value, bool,       boolean_t)
DECL_PAIR2(pair2char, nvpair_value_uint8,         char,       uint8_t)
DECL_PAIR2(pair2int,  nvpair_value_uint64,        uint64_t,   uint64_t)
DECL_PAIR2(pair2nvl,  nvpair_value_nvlist,        nvlist_t *, nvlist_t *)
