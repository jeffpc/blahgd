#include <stdlib.h>

#include "utils.h"
#include "vars.h"
#include "qstring.h"

static int onefile(char *ibuf, size_t len)
{
	nvlist_t *vars;

	vars = nvl_alloc();

	parse_query_string(vars, ibuf, len);

	nvlist_free(vars);

	return 0;
}

int main(int argc, char **argv)
{
	char *in;
	int i;
	int result;

	result = 0;

	ASSERT0(putenv("UMEM_DEBUG=default,verbose"));
	ASSERT0(putenv("BLAHG_DISABLE_SYSLOG=1"));

	for (i = 1; i < argc; i++) {
		in = read_file(argv[i]);
		ASSERT(in);

		if (onefile(in, strlen(in)))
			result = 1;

		free(in);
	}

	return result;
}
