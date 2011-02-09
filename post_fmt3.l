%option nounput
%option noinput

%{
#include <stdlib.h>

#include "post.h"

struct post *post;

enum {
	PAREND = 1,
	WSPACE,
	BSLASH,
	OCURLY,
	CCURLY,
	DASH,
	OQUOT,
	CQUOT,
	SCHAR,
	WORD,
};
%}

%%
\n\n			{ fprintf(post->out, "tok:PAREND\n"); return PAREND; }
\n			{ fprintf(post->out, "tok:WSPACE\n"); return WSPACE; }
[ \t]			{ fprintf(post->out, "tok:WSPACE\n"); return WSPACE; }
"\\"			{ fprintf(post->out, "tok:BSLASH\n"); return BSLASH; }
"{"			{ fprintf(post->out, "tok:OCURLY\n"); return OCURLY; }
"}"			{ fprintf(post->out, "tok:CCURLY\n"); return CCURLY; }
-{1,3}			{ fprintf(post->out, "tok:DASH '%s'\n", yytext); return DASH; }
`{1,2}			{ fprintf(post->out, "tok:OQUOT '%s'\n", yytext); return OQUOT; }
'{1,2}			{ fprintf(post->out, "tok:CQUOT '%s'\n", yytext); return CQUOT; }
[".,()/=<>!:;_\$]	{ fprintf(post->out, "tok:SCHAR '%s'\n", yytext); return SCHAR; }
[A-Za-z0-9]+		{ fprintf(post->out, "tok:WORD '%s'\n", yytext); return WORD; }
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
