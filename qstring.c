/*
 * Copyright (c) 2014-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/error.h>

#include "vars.h"
#include "decode.h"
#include "qstring.h"
#include "utils.h"

enum {
	QS_STATE_ERROR = 0,
	QS_STATE_NAME,
	QS_STATE_VAL,
};

struct qstr {
	size_t len;
	size_t idx;
	char buf[0];
};

static struct qstr *alloc_str(size_t size)
{
	size_t allocsize = size * 3; /* FIXME: is 3x good enough? */
	struct qstr *tmp;

	tmp = malloc(sizeof(struct qstr) + allocsize);
	if (!tmp)
		return NULL;

	memset(tmp, 0, allocsize);

	tmp->len = allocsize;
	tmp->idx = 0;

	return tmp;
}

static void free_str(struct qstr *str)
{
	free(str);
}

static int append_char_str(struct qstr *str, char c)
{
	if ((str->idx + 1) == str->len)
		return -E2BIG;

	str->buf[str->idx++] = c;

	return 0;
}

static void reset_str(struct qstr *str)
{
	str->idx = 0;
}

static void insert(nvlist_t *vars, struct qstr *name, struct qstr *val)
{
	urldecode(name->buf, name->idx, name->buf);
	urldecode(val->buf, val->idx, val->buf);

	/* now, {name,val}->buf are null-terminated strings */

	nvl_set_str(vars, name->buf, val->buf);
}

void parse_query_string_len(nvlist_t *vars, const char *qs, size_t len)
{
	struct qstr *name, *val;
	const char *cur, *end;
	int state;

	if (!qs || !len)
		return;

	name = alloc_str(VAR_NAME_MAXLEN);
	val  = alloc_str(64 * 1024);

	ASSERT(name);
	ASSERT(val);

	state = QS_STATE_NAME;

	end = qs + len;
	cur = qs;

	for (; end > cur; cur++) {
		char c = *cur;

		if (state == QS_STATE_NAME) {
			if (c == '=')
				state = QS_STATE_VAL;
			else
				ASSERT0(append_char_str(name, c));
		} else if (state == QS_STATE_VAL) {
			if (c == '&') {
				insert(vars, name, val);

				state = QS_STATE_NAME;
				reset_str(name);
				reset_str(val);
			} else {
				ASSERT0(append_char_str(val, c));
			}
		}
	}

	if (state == QS_STATE_VAL)
		insert(vars, name, val);

	free_str(name);
	free_str(val);
}
