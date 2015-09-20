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

#ifndef __LISP_H
#define __LISP_H

#include <stdbool.h>

#include "val.h"
#include "str.h"

extern struct val *parse_lisp(const char *str, size_t len);
extern struct str *lisp_dump(struct val *lv, bool raw);
extern void lisp_dump_file(FILE *out, struct val *lv, bool raw);

static inline struct val *parse_lisp_str(struct str *str)
{
	if (!str)
		return NULL;

	return parse_lisp(str->str, str_len(str));
}

extern struct val *lisp_car(struct val *val);
extern struct val *lisp_cdr(struct val *val);
extern struct val *lisp_assoc(struct val *val, const char *name);

#endif