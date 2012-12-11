#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "listing.h"

#define PAGE 4096

#define COPYSTR(ptr, len, size, str)	do { \
						int x = strlen(str); \
						if ((len+x+1) > size) { \
							size += PAGE; \
							ptr = realloc(ptr, size); \
						} \
						strcpy(ptr+len, str); \
						len += x; \
					} while(0)

#define COPYCHR(ptr, len, size, chr)	do { \
						if ((len+2) > size) { \
							size += PAGE; \
							ptr = realloc(ptr, size); \
						} \
						ptr[len] = chr; \
						ptr[len+1] = '\0'; \
						len++; \
					} while(0)

char *listing(struct post_old *post, char *fname)
{
	char path[FILENAME_MAX];
	char *out;
	int len, size;
	FILE *in;

	snprintf(path, FILENAME_MAX, "%s/%s", post->path, fname);

	in = fopen(path, "r");
	if (!in) {
		snprintf(path, FILENAME_MAX, "Failed to open listing file '%s'.", fname);
		return strdup(path);
	}

	len = 0;
	size = PAGE;
	out = malloc(PAGE);
	if (!out)
		goto err_mem;

	while(1) {
		char c;

		c = fgetc(in);
		if (feof(in))
			break;

		switch(c) {
			case '&':
				COPYSTR(out, len, size, "&amp;");
				break;
			case '<':
				COPYSTR(out, len, size, "&lt;");
				break;
			case '>':
				COPYSTR(out, len, size, "&gt;");
				break;
			default:
				COPYCHR(out, len, size, c);
				break;
		}
	}

	fclose(in);
	return out;

err_mem:
	snprintf(path, FILENAME_MAX, "Falied to allocate memory for buffer "
		 "(len %d, size %d bytes).", len, size);
	return strdup(path);
}
