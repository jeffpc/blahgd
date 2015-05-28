%option nounput
%option noinput
%option reentrant
%option bison-bridge
%option never-interactive

%{
#include <stdlib.h>

#include "parse.h"
#include "str.h"

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
<MATH>[=<>]		{ yylval->ptr = STR_DUP(yytext); return EQLTGT; }
<MATH>[A-Za-z0-9]+	{ yylval->ptr = STR_DUP(yytext); return WORD; }
<MATH>[".,=<>!:;?@*#]	{ yylval->ptr = STR_DUP(yytext); return SCHAR; }
<MATH>[ \t]+		{ yylval->ptr = STR_DUP(yytext); return WSPACE; }

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
