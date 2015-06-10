#include <stdio.h>
#include <libnvpair.h>

#include "nvl.h"
#include "utils.h"

static void dump_file(char *fname)
{
	nvlist_t *nvl;
	size_t len;
	char *buf;
	int ret;

	buf = read_file_len(fname, &len);
	if (!buf) {
		fprintf(stderr, "Error: could not read file: %s\n", fname);
		return;
	}

	ret = nvlist_unpack(buf, len, &nvl, 0);
	if (ret) {
		fprintf(stderr, "Error: failed to parse file: %s\n", fname);
		goto out;
	}

	nvl_dump(nvl);

	nvlist_free(nvl);

out:
	free(buf);
}

int main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
		dump_file(argv[i]);

	return 0;
}
