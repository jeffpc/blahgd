#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "listing.h"
#include "utils.h"
#include "mangle.h"

#define PAGE 4096

char *listing(struct post *post, char *fname)
{
	char path[FILENAME_MAX];
	char *in;
	char *tmp;

	snprintf(path, FILENAME_MAX, "data/posts/%d/%s", post->id, fname);

	in = read_file(path);
	if (!in)
		goto err;

	tmp = mangle_htmlescape(in);

	free(in);

	return tmp;

err:
	snprintf(path, FILENAME_MAX, "Failed to read in listing '%d/%s'",
		 post->id, fname);
	return xstrdup(path);
}
