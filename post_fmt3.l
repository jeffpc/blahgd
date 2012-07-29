%option nounput
%option noinput

%{
#include <stdlib.h>

#include "post_fmt3.tab.h"

extern void fmt3_error2(char *, char *);

%}

%%
\n\n[ \t\n]*		{ fmt3_lval.ptr = strdup(yytext); return PAREND; }
\n			{ fmt3_lval.ptr = strdup(yytext); return NLINE; }
[ \t]+			{ fmt3_lval.ptr = strdup(yytext); return WSPACE; }
"\\"			{ fmt3_lval.ptr = strdup(yytext); return BSLASH; }
"{"			{ fmt3_lval.ptr = strdup(yytext); return OCURLY; }
"}"			{ fmt3_lval.ptr = strdup(yytext); return CCURLY; }
"["			{ fmt3_lval.ptr = strdup(yytext); return OBRACE; }
"]"			{ fmt3_lval.ptr = strdup(yytext); return CBRACE; }
"&"			{ fmt3_lval.ptr = strdup(yytext); return AMP; }
"_"			{ fmt3_lval.ptr = strdup(yytext); return USCORE; }
"%"			{ fmt3_lval.ptr = strdup(yytext); return PERCENT; }
"$"			{ fmt3_lval.ptr = strdup(yytext); return DOLLAR; }
"~"			{ fmt3_lval.ptr = strdup(yytext); return TILDE; }
"|"			{ fmt3_lval.ptr = strdup(yytext); return PIPE; }
\.{3}			{ fmt3_lval.ptr = strdup(yytext); return ELLIPSIS; }
-{1,3}			{ fmt3_lval.ptr = strdup(yytext); return DASH; }
`{1,2}			{ fmt3_lval.ptr = strdup(yytext); return OQUOT; }
'{1,2}			{ fmt3_lval.ptr = strdup(yytext); return CQUOT; }
[".,()/=<>!:;\+?@*#]	{ fmt3_lval.ptr = strdup(yytext); return SCHAR; }
[\xe0-\xef]		{ fmt3_lval.ptr = strdup(yytext); return UTF8FIRST3; }
[\xc0-\xdf]		{ fmt3_lval.ptr = strdup(yytext); return UTF8FIRST2; }
[\x80-\xbf]		{ fmt3_lval.ptr = strdup(yytext); return UTF8REST; }
[A-Za-z0-9]+		{ fmt3_lval.ptr = strdup(yytext); return WORD; }
.|\n			{ fmt3_error2("post text contains invalid characters", yytext); yyterminate(); }
%%

int yywrap()
{
	fclose(yyin);
	return 1;
}
