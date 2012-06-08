%option nounput
%option noinput

%{
#include <stdlib.h>

#include "post_fmt3.tab.h"

extern void yyerror2(char *, char *);

%}

%%
\n\n[ \t\n]*		{ yylval.ptr = strdup(yytext); return PAREND; }
\n			{ yylval.ptr = strdup(yytext); return NLINE; }
[ \t]+			{ yylval.ptr = strdup(yytext); return WSPACE; }
"\\"			{ yylval.ptr = strdup(yytext); return BSLASH; }
"{"			{ yylval.ptr = strdup(yytext); return OCURLY; }
"}"			{ yylval.ptr = strdup(yytext); return CCURLY; }
"["			{ yylval.ptr = strdup(yytext); return OBRACE; }
"]"			{ yylval.ptr = strdup(yytext); return CBRACE; }
"&"			{ yylval.ptr = strdup(yytext); return AMP; }
"_"			{ yylval.ptr = strdup(yytext); return USCORE; }
"%"			{ yylval.ptr = strdup(yytext); return PERCENT; }
"$"			{ yylval.ptr = strdup(yytext); return DOLLAR; }
"~"			{ yylval.ptr = strdup(yytext); return TILDE; }
"|"			{ yylval.ptr = strdup(yytext); return PIPE; }
\.{3}			{ yylval.ptr = strdup(yytext); return ELLIPSIS; }
"+"			{ yylval.ptr = strdup(yytext); return PLUS; }
-			{ yylval.ptr = strdup(yytext); return MINUS; }
"*"			{ yylval.ptr = strdup(yytext); return ASTERISK; }
"/"			{ yylval.ptr = strdup(yytext); return SLASH; }
-{2,3}			{ yylval.ptr = strdup(yytext); return DASH; }
`{1,2}			{ yylval.ptr = strdup(yytext); return OQUOT; }
'{1,2}			{ yylval.ptr = strdup(yytext); return CQUOT; }
[".,()=<>!:;\?@#]	{ yylval.ptr = strdup(yytext); return SCHAR; }
[\xe0-\xef]		{ yylval.ptr = strdup(yytext); return UTF8FIRST3; }
[\xc0-\xdf]		{ yylval.ptr = strdup(yytext); return UTF8FIRST2; }
[\x80-\xbf]		{ yylval.ptr = strdup(yytext); return UTF8REST; }
"times"			{ yylval.ptr = strdup(yytext); return W_TIMES; }
"frac"			{ yylval.ptr = strdup(yytext); return W_FRAC; }
[A-Za-z0-9]+		{ yylval.ptr = strdup(yytext); return WORD; }
.|\n			{ yyerror2("post text contains invalid characters", yytext); yyterminate(); }
%%

int yywrap()
{
	fclose(yyin);
	return 1;
}
