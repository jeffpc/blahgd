#include <stdlib.h>
#include <string.h>

#include "mangle.h"

char *mangle_htmlescape(char *in)
{
	char *out, *tmp;
	int outlen;
	char *s;

	outlen = strlen(in);

	for (s = in; *s; s++) {
		char c = *s;

		switch (c) {
			case '<':
			case '>':
				/* "&lt;" "&gt;" */
				outlen += 3;
				break;
			case '&':
				/* "&amp;" */
				outlen += 4;
				break;
		}
	}

	out = malloc(outlen + 1);
	if (!out)
		return NULL;

	for (s = in, tmp = out; *s; s++, tmp++) {
		unsigned char c = *s;

		switch (c) {
			case '<':
				strcpy(tmp, "&lt;");
				tmp += 3;
				break;
			case '>':
				strcpy(tmp, "&gt;");
				tmp += 3;
				break;
			case '&':
				strcpy(tmp, "&amp;");
				tmp += 4;
				break;
			default:
				*tmp = c;
				break;
		}
	}

	*tmp = '\0';

	return out;
}
