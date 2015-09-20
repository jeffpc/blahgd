/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "parse.h"
#include "lisp.h"

struct val *parse_lisp(const char *str, size_t len)
{
	struct parser_output x;

	x.req   = NULL;
	x.post  = NULL;
	x.input = str;
	x.len   = len;
	x.pos   = 0;

	lisp_reader_lex_init(&x.scanner);
	lisp_reader_set_extra(&x, x.scanner);

	ASSERT(lisp_reader_parse(&x) == 0);

	lisp_reader_lex_destroy(x.scanner);

	return x.valoutput;
}

static struct str *dump_cons(struct val *lv, bool raw)
{
	struct val *head = lv->cons.head;
	struct val *tail = lv->cons.tail;

	if (raw)
		return str_cat3(lisp_dump(head, raw),
				STR_DUP(" . "),
				lisp_dump(tail, raw));
	else if (!head && !tail)
		return NULL;
	else if (head && !tail)
		return lisp_dump(head, raw);
	else if (tail->type == VT_CONS)
		return str_cat3(lisp_dump(head, raw),
				STR_DUP(" "),
				dump_cons(tail, raw));
	else
		return str_cat5(STR_DUP("("),
				lisp_dump(head, raw),
				STR_DUP(" . "),
				lisp_dump(tail, raw),
				STR_DUP(")"));
}

struct str *lisp_dump(struct val *lv, bool raw)
{
	if (!lv)
		return STR_DUP("()");

	switch (lv->type) {
		case VT_SYM:
			return STR_DUP(lv->cstr);
		case VT_CSTR:
			return str_cat3(STR_DUP("\""),
					STR_DUP(lv->cstr),
					STR_DUP("\""));
		case VT_BOOL:
			return lv->b ? STR_DUP("#t") : STR_DUP("#f");
		case VT_INT: {
			char tmp[32];

			snprintf(tmp, sizeof(tmp), "%"PRIu64, lv->i);

			return STR_DUP(tmp);
		}
		case VT_CONS:
			return str_cat3(STR_DUP("("),
					dump_cons(lv, raw),
					STR_DUP(")"));
	}

	return NULL;
}

void lisp_dump_file(FILE *out, struct val *lv, bool raw)
{
	struct str *tmp;

	tmp = lisp_dump(lv, raw);

	fprintf(out, "%s", tmp->str);

	str_putref(tmp);
}
