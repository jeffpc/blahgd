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

#include <stdlib.h>
#include <string.h>

#include "mangle.h"

char *mangle_htmlescape(const char *in)
{
	char *out, *tmp;
	int outlen;
	const char *s;

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

char *mangle_lispescape(const char *in)
{
	char *out, *tmp;
	const char *s;
	size_t outlen;

	outlen = strlen(in);

	for (s = in; *s; s++) {
		char c = *s;

		switch (c) {
			case '\n':
			case '\t':
			case '\r':
			case '\b':
			case '\f':
			case '"':
				/* "\n", "\t", ... */
				outlen++;
				break;
		}
	}

	out = malloc(outlen + 1);
	if (!out)
		return NULL;

	for (s = in, tmp = out; *s; s++, tmp++) {
		unsigned char c = *s;

		switch (c) {
			case '\n':
				strcpy(tmp, "\\n");
				tmp++;
				break;
			case '\t':
				strcpy(tmp, "\\t");
				tmp++;
				break;
			case '\r':
				strcpy(tmp, "\\r");
				tmp++;
				break;
			case '\b':
				strcpy(tmp, "\\b");
				tmp++;
				break;
			case '\f':
				strcpy(tmp, "\\f");
				tmp++;
				break;
			case '"':
				strcpy(tmp, "\\\"");
				tmp++;
				break;
			default:
				*tmp = c;
				break;
		}
	}

	*tmp = '\0';

	return out;
}
