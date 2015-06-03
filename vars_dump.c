#include <sys/inttypes.h>

#include "error.h"
#include "vars_impl.h"

#define INDENT	4

char *pair2str(nvpair_t *pair)
{
	char *out;
	int ret;

	ret = nvpair_value_string(pair, &out);
	ASSERT0(ret);

	return out;
}

bool pair2bool(nvpair_t *pair)
{
	boolean_t out;
	int ret;

	ret = nvpair_value_boolean_value(pair, &out);
	ASSERT0(ret);

	return out;
}

char pair2char(nvpair_t *pair)
{
	uint8_t out;
	int ret;

	ret = nvpair_value_uint8(pair, &out);
	ASSERT0(ret);

	return (char) out;
}

uint64_t pair2int(nvpair_t *pair)
{
	uint64_t out;
	int ret;

	ret = nvpair_value_uint64(pair, &out);
	ASSERT0(ret);

	return out;
}

nvlist_t *pair2nvl(nvpair_t *pair)
{
	nvlist_t *out;
	int ret;

	ret = nvpair_value_nvlist(pair, &out);
	ASSERT0(ret);

	return out;
}

static void __dump(nvlist_t *list, int indent);

static void __dump_string_array(nvpair_t *pair, int indent)
{
	char **out;
	uint_t nout;
	uint_t i;
	int ret;

	ret = nvpair_value_string_array(pair, &out, &nout);
	ASSERT0(ret);

	fprintf(stderr, " items=%u\n%*svalue=", nout, indent, "");

	for (i = 0; i < nout; i++)
		fprintf(stderr, "%s'%s'", i ? " + " : "", out[i]);

	fprintf(stderr, "\n");
}

static void __dump_nvlist_array(nvpair_t *pair, int indent)
{
	nvlist_t **out;
	uint_t nout;
	uint_t i;
	int ret;

	ret = nvpair_value_nvlist_array(pair, &out, &nout);
	ASSERT0(ret);

	fprintf(stderr, " items=%u\n", nout);

	for (i = 0; i < nout; i++) {
		fprintf(stderr, "%*s%u:\n", indent, "", i);
		__dump(out[i], indent + INDENT);
	}
}

static void __dump(nvlist_t *list, int indent)
{
	nvpair_t *pair;

	for (pair = nvlist_next_nvpair(list, NULL);
	     pair;
	     pair = nvlist_next_nvpair(list, pair)) {
		fprintf(stderr, "%*sname='%s' type=%d", indent, "",
			nvpair_name(pair), nvpair_type(pair));

		switch (nvpair_type(pair)) {
			case DATA_TYPE_STRING:
				fprintf(stderr, "\n%*svalue='%s'\n",
					indent + INDENT, "", pair2str(pair));
				break;
			case DATA_TYPE_STRING_ARRAY:
				__dump_string_array(pair, indent + INDENT);
				break;
			case DATA_TYPE_BOOLEAN:
				fprintf(stderr, "\n%*svalue=%s\n",
					indent + INDENT, "",
					pair2bool(pair) ? "true" : "false");
				break;
			case DATA_TYPE_UINT8:
				fprintf(stderr, "\n%*svalue='%c'\n",
					indent + INDENT, "", pair2char(pair));
				break;
			case DATA_TYPE_UINT64:
				fprintf(stderr, "\n%*svalue=%"PRIu64"\n",
					indent + INDENT, "", pair2int(pair));
				break;
			case DATA_TYPE_NVLIST:
				fprintf(stderr, "\n");
				__dump(pair2nvl(pair), indent + INDENT);
				break;
			case DATA_TYPE_NVLIST_ARRAY:
				__dump_nvlist_array(pair, indent + INDENT);
				break;
			default:
				fprintf(stderr, "\n");
				break;
		}
	}
}

void nvl_dump(nvlist_t *nvl)
{
	fprintf(stderr, "nvlist dump @ %p\n", nvl);
	__dump(nvl, INDENT);
}

void vars_dump(struct vars *vars)
{
	int i;

	for (i = 0; i <= vars->cur; i++) {
		fprintf(stderr, "VARS DUMP scope %d @ %p\n", i,
			vars->scopes[i]);
		__dump(vars->scopes[i], INDENT);
	}
}
