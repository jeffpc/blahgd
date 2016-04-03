/*
 * Copyright (c) 2011-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/str.h>

#include "parse.h"

#include "post_fmt3.tab.h"

extern void fmt3_error2(char *, char *);

#define YY_INPUT(buf, result, max_size)	\
	result = fmt3_input_proc(buf, max_size, yyscanner)
int fmt3_input_proc(char *buf, int size, yyscan_t scanner);

static void inc_lineno(yyscan_t scanner);
%}

%x VERBATIM
%x LISTING
%x MATH

%%
"\\begin{verbatim}"		{ BEGIN(VERBATIM); return VERBSTART; }
<VERBATIM>"\\end{verbatim}"	{ BEGIN(INITIAL); return VERBEND; }
<VERBATIM>.			{ yylval->ptr = STR_DUP(yytext); return VERBTEXT; }
<VERBATIM>\n			{
					yylval->ptr = STR_DUP(yytext);
					inc_lineno(yyscanner);
					return VERBTEXT;
				}

"\\begin{listing}"		{ BEGIN(LISTING); return LISTSTART; }
<LISTING>"\\end{listing}"	{ BEGIN(INITIAL); return LISTEND; }
<LISTING>.			{ yylval->ptr = STR_DUP(yytext); return VERBTEXT; }
<LISTING>\n			{
					yylval->ptr = STR_DUP(yytext);
					inc_lineno(yyscanner);
					return VERBTEXT;
				}

"$"			{ BEGIN(MATH); return MATHSTART; }
<MATH>"$"		{ BEGIN(INITIAL); return MATHEND; }
<MATH>"\\"		{ return BSLASH; }
<MATH>"_"		{ return USCORE; }
<MATH>"^"		{ yylval->ptr = STR_DUP(yytext); return CARRET; }
<MATH>"+"		{ yylval->ptr = STR_DUP(yytext); return PLUS; }
<MATH>"-"		{ yylval->ptr = STR_DUP(yytext); return MINUS; }
<MATH>"*"		{ return ASTERISK; }
<MATH>"/"		{ return SLASH; }
<MATH>"{"		{ return OCURLY; }
<MATH>"}"		{ return CCURLY; }
<MATH>"("		{ return OPAREN; }
<MATH>")"		{ return CPAREN; }
<MATH>"~"		{ return TILDE; }
<MATH>[=<>]		{ yylval->ptr = STR_DUP(yytext); return EQLTGT; }
<MATH>[A-Za-z0-9]+	{ yylval->ptr = STR_DUP(yytext); return WORD; }
<MATH>[".,=<>!:;?@*#]	{ yylval->ptr = STR_DUP(yytext); return SCHAR; }
<MATH>[ \t]+		{ yylval->ptr = STR_DUP(yytext); return WSPACE; }
<MATH>.			{ fmt3_error2("post math contains invalid characters", yytext); yyterminate(); }

"\\$"			{ return DOLLAR; }
\n\n[ \t\n]*		{
				int i;

				for (i = 0; i < strlen(yytext); i++)
					if (yytext[i] == '\n')
						inc_lineno(yyscanner);
				return PAREND;
			}
\n			{
				inc_lineno(yyscanner);
				return NLINE;
			}
%[^\n]*			{ /* tex comment */ }
[ \t]+			{ yylval->ptr = STR_DUP(yytext); return WSPACE; }
"\\"			{ return BSLASH; }
"{"			{ return OCURLY; }
"}"			{ return CCURLY; }
"["			{ return OBRACE; }
"]"			{ return CBRACE; }
"&"			{ return AMP; }
"_"			{ return USCORE; }
"\\%"			{ return PERCENT; }
"~"			{ return TILDE; }
"|"			{ return PIPE; }
\.{3}			{ return ELLIPSIS; }
-{1,3}			{ yylval->ptr = STR_DUP(yytext); return DASH; }
`{1,2}			{ yylval->ptr = STR_DUP(yytext); return OQUOT; }
'{1,2}			{ yylval->ptr = STR_DUP(yytext); return CQUOT; }
[".,()/=<>!:;\+?@*#]	{ yylval->ptr = STR_DUP(yytext); return SCHAR; }
[\xe0-\xef]		{ yylval->ptr = STR_DUP(yytext); return UTF8FIRST3; }
[\xc0-\xdf]		{ yylval->ptr = STR_DUP(yytext); return UTF8FIRST2; }
[\x80-\xbf]		{ yylval->ptr = STR_DUP(yytext); return UTF8REST; }
[A-Za-z0-9]+		{ yylval->ptr = STR_DUP(yytext); return WORD; }
.			{ fmt3_error2("post text contains invalid characters", yytext); yyterminate(); }
%%

static void inc_lineno(yyscan_t scanner)
{
	struct parser_output *out;

	out = (struct parser_output *) fmt3_get_extra(scanner);

	out->lineno++;
}

int fmt3_input_proc(char *buf, int size, yyscan_t scanner)
{
	struct parser_output *out;
	int num;

	out = (struct parser_output *) fmt3_get_extra(scanner);
	num = out->len - out->pos;

	if (num <= 0)
		return 0;

	if (num > size)
		num = size;

	memcpy(buf, out->input + out->pos, num);
	out->pos += num;

	return num;
}

int fmt3_wrap(yyscan_t scanner)
{
	return 1;
}
