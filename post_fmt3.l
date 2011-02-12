%option nounput
%option noinput

%{
#include <stdlib.h>

#include "post_fmt3.tab.h"

#include "post.h"

static struct post *post;

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
-{1,3}			{ yylval.ptr = strdup(yytext); return DASH; }
`{1,2}			{ yylval.ptr = strdup(yytext); return OQUOT; }
'{1,2}			{ yylval.ptr = strdup(yytext); return CQUOT; }
[".,()/=<>!:;\$]	{ yylval.ptr = strdup(yytext); return SCHAR; }
[A-Za-z0-9]+		{ yylval.ptr = strdup(yytext); return WORD; }
%%

void __do_cat_post_fmt3(struct post *p, char *path)
{
	yyin = fopen(path, "r");
	if (!yyin) {
		fprintf(post->out, "post.txt open error\n");
		return;
	}

	post = p;

	while (yylex())
		;
}

int yywrap()
{
	fclose(yyin);
	return 1;
}
