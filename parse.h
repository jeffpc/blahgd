/*
 * Copyright (c) 2012-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#ifndef __PARSE_H
#define __PARSE_H

#include <jeffpc/error.h>
#include <jeffpc/str.h>
#include <jeffpc/sexpr.h>

#include "req.h"
#include "post.h"

struct parser_output {
	struct req *req;
	struct post *post;

	void *scanner;
	char *output;
	struct str *stroutput;
	struct val *valoutput;

	const char *input;
	size_t len;
	size_t pos;
	int lineno;

	/* template related */
	bool cond_stack[COND_STACK_DEPTH];
	int cond_stack_use;

	/* fmt3 */
	int table_nesting;
	int texttt_nesting;

	/* fmt3 special commands */
	struct str *sc_title;
	struct str *sc_pub;
	struct val *sc_tags;
	struct val *sc_cats;
};

typedef void* yyscan_t;

extern int fmt3_lex_destroy(yyscan_t yyscanner);
extern int tmpl_lex_destroy(yyscan_t yyscanner);
extern int fmt3_parse(struct parser_output *data);
extern int tmpl_parse(struct parser_output *data);
extern void fmt3_set_extra(void * user_defined, yyscan_t yyscanner);
extern void tmpl_set_extra(void * user_defined, yyscan_t yyscanner);
extern int fmt3_lex_init(yyscan_t* scanner);
extern int tmpl_lex_init(yyscan_t* scanner);

static inline bool cond_value(struct parser_output *data)
{
	int i;

	for (i = 0; i <= data->cond_stack_use; i++)
		if (!data->cond_stack[i])
			return false;

	return true;
}

static inline void cond_if(struct parser_output *data, bool val)
{
	ASSERT3S(data->cond_stack_use, <, COND_STACK_DEPTH);

	data->cond_stack[++data->cond_stack_use] = val;
}

static inline void cond_else(struct parser_output *data)
{
	ASSERT3S(data->cond_stack_use, >=, 0);

	data->cond_stack[data->cond_stack_use] = !data->cond_stack[data->cond_stack_use];
}

static inline void cond_endif(struct parser_output *data)
{
	ASSERT3S(data->cond_stack_use, >=, 0);

	data->cond_stack_use--;
}

#endif
