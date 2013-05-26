%option nounput
%option noinput
%option reentrant
%option bison-bridge

%{
#include <stdlib.h>

#include "parse.h"
#include "utils.h"

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
<VERBATIM>.			{ yylval->ptr = xstrdup(yytext); return VERBTEXT; }
<VERBATIM>\n			{
					yylval->ptr = xstrdup(yytext);
					inc_lineno(yyscanner);
					return VERBTEXT;
				}

"\\begin{listing}"		{ BEGIN(LISTING); return LISTSTART; }
<LISTING>"\\end{listing}"	{ BEGIN(INITIAL); return LISTEND; }
<LISTING>.			{ yylval->ptr = xstrdup(yytext); return VERBTEXT; }
<LISTING>\n			{
					yylval->ptr = xstrdup(yytext);
					inc_lineno(yyscanner);
					return VERBTEXT;
				}

"$"			{ BEGIN(MATH); return MATHSTART; }
<MATH>"$"		{ BEGIN(INITIAL); return MATHEND; }
<MATH>"\\"		{ yylval->ptr = xstrdup(yytext); return BSLASH; }
<MATH>"_"		{ yylval->ptr = xstrdup(yytext); return USCORE; }
<MATH>"^"		{ yylval->ptr = xstrdup(yytext); return CARRET; }
<MATH>"+"		{ yylval->ptr = xstrdup(yytext); return PLUS; }
<MATH>"-"		{ yylval->ptr = xstrdup(yytext); return MINUS; }
<MATH>"*"		{ yylval->ptr = xstrdup(yytext); return ASTERISK; }
<MATH>"/"		{ yylval->ptr = xstrdup(yytext); return SLASH; }
<MATH>"{"		{ yylval->ptr = xstrdup(yytext); return OCURLY; }
<MATH>"}"		{ yylval->ptr = xstrdup(yytext); return CCURLY; }
<MATH>"("		{ yylval->ptr = xstrdup(yytext); return OPAREN; }
<MATH>")"		{ yylval->ptr = xstrdup(yytext); return CPAREN; }
<MATH>[=<>]		{ yylval->ptr = xstrdup(yytext); return EQLTGT; }
<MATH>[A-Za-z0-9]+	{ yylval->ptr = xstrdup(yytext); return WORD; }
<MATH>[".,=<>!:;?@*#]	{ yylval->ptr = xstrdup(yytext); return SCHAR; }
<MATH>[ \t]+		{ yylval->ptr = xstrdup(yytext); return WSPACE; }

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
[ \t]+			{ yylval->ptr = xstrdup(yytext); return WSPACE; }
"\\"			{ yylval->ptr = xstrdup(yytext); return BSLASH; }
"{"			{ yylval->ptr = xstrdup(yytext); return OCURLY; }
"}"			{ yylval->ptr = xstrdup(yytext); return CCURLY; }
"["			{ yylval->ptr = xstrdup(yytext); return OBRACE; }
"]"			{ yylval->ptr = xstrdup(yytext); return CBRACE; }
"&"			{ yylval->ptr = xstrdup(yytext); return AMP; }
"_"			{ yylval->ptr = xstrdup(yytext); return USCORE; }
"\\%"			{ yylval->ptr = xstrdup(yytext+1); return PERCENT; }
"~"			{ yylval->ptr = xstrdup(yytext); return TILDE; }
"|"			{ yylval->ptr = xstrdup(yytext); return PIPE; }
\.{3}			{ yylval->ptr = xstrdup(yytext); return ELLIPSIS; }
-{1,3}			{ yylval->ptr = xstrdup(yytext); return DASH; }
`{1,2}			{ yylval->ptr = xstrdup(yytext); return OQUOT; }
'{1,2}			{ yylval->ptr = xstrdup(yytext); return CQUOT; }
[".,()/=<>!:;\+?@*#]	{ yylval->ptr = xstrdup(yytext); return SCHAR; }
[\xe0-\xef]		{ yylval->ptr = xstrdup(yytext); return UTF8FIRST3; }
[\xc0-\xdf]		{ yylval->ptr = xstrdup(yytext); return UTF8FIRST2; }
[\x80-\xbf]		{ yylval->ptr = xstrdup(yytext); return UTF8REST; }
[A-Za-z0-9]+		{ yylval->ptr = xstrdup(yytext); return WORD; }
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
	struct yyguts_t * yyg = (struct yyguts_t*) scanner;

	fclose(yyin);
	return 1;
}
