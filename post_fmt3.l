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
%}

%%
\n\n[ \t\n]*		{ yylval->ptr = xstrdup(yytext); return PAREND; }
\n			{ yylval->ptr = xstrdup(yytext); return NLINE; }
[ \t]+			{ yylval->ptr = xstrdup(yytext); return WSPACE; }
"\\"			{ yylval->ptr = xstrdup(yytext); return BSLASH; }
"{"			{ yylval->ptr = xstrdup(yytext); return OCURLY; }
"}"			{ yylval->ptr = xstrdup(yytext); return CCURLY; }
"["			{ yylval->ptr = xstrdup(yytext); return OBRACE; }
"]"			{ yylval->ptr = xstrdup(yytext); return CBRACE; }
"&"			{ yylval->ptr = xstrdup(yytext); return AMP; }
"_"			{ yylval->ptr = xstrdup(yytext); return USCORE; }
"%"			{ yylval->ptr = xstrdup(yytext); return PERCENT; }
"$"			{ yylval->ptr = xstrdup(yytext); return DOLLAR; }
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
.|\n			{ fmt3_error2("post text contains invalid characters", yytext); yyterminate(); }
%%

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
