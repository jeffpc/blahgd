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
[{}|%]			{ return *yytext; }
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
