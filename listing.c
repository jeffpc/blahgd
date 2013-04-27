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

	snprintf(path, FILENAME_MAX, "data/posts/%d/%s", post->id, fname);

	in = read_file(path);
	if (!in)
		goto err;

	return listing_str(in);

err:
	snprintf(path, FILENAME_MAX, "Failed to read in listing '%d/%s'",
		 post->id, fname);
	return xstrdup(path);
}
