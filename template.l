/*
 * Copyright (c) 2012-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

%option nounput
%option noinput
%option reentrant
%option bison-bridge
%option never-interactive

%{
#include <stdlib.h>

#include "parse.h"
#include "utils.h"

#include "template.tab.h"

#define YY_INPUT(buf, result, max_size)	\
	result = tmpl_input_proc(buf, max_size, yyscanner)
int tmpl_input_proc(char *buf, int size, yyscan_t scanner);

static void inc_lineno(yyscan_t scanner);
%}

%%
[{}|%(),]		{ return *yytext; }
[A-Za-z0-9_]+		{ yylval->ptr = xstrdup(yytext); return WORD; }
[ \r\t]			{ yylval->c = *yytext; return CHAR; }
\n			{
				yylval->c = *yytext;
				inc_lineno(yyscanner);
				return CHAR;
			}
.			{ yylval->c = *yytext; return CHAR; }
%%

static void inc_lineno(yyscan_t scanner)
{
	struct parser_output *out;

	out = (struct parser_output *) tmpl_get_extra(scanner);

	out->lineno++;
}

int tmpl_input_proc(char *buf, int size, yyscan_t scanner)
{
	struct parser_output *out;
	int num;

	out = (struct parser_output *) tmpl_get_extra(scanner);
	num = out->len - out->pos;

	if (num <= 0)
		return 0;

	if (num > size)
		num = size;

	memcpy(buf, out->input + out->pos, num);
	out->pos += num;

	return num;
}

int tmpl_wrap(yyscan_t scanner)
{
	return 1;
}
