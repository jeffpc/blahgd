%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"

#include "parse.h"

#define scanner data->scanner

extern char *render_template(struct req *req, const char *tmpl);
extern int tmpl_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	fprintf(stderr, "Error: %s\n", e);
}

static char *tostr(char c)
{
	char *ret;

	ret = malloc(2);
	assert(ret);

	ret[0] = c;
	ret[1] = '\0';

	return ret;
}

static char *concat(char *a, char *b)
{
	char *ret;

	ret = malloc(strlen(a) + strlen(b) + 1);
	assert(ret);

	strcpy(ret, a);
	strcat(ret, b);

	free(a);
	free(b);

	return ret;
}

char *foreach(struct req *req, char *var, char *tmpl)
{
	char *ret;

	ret = strdup("");

	if (!strcmp(var, "posts"))
		ret = concat(ret, strdup("XXX"));

	return ret;
}
%}

%union {
	char c;
	char *ptr;
};

%token <c> OCURLY CCURLY PIPE FORMAT
%token <c> LETTER CHAR

%type <ptr> words cmd pipeline pipe name

%%

page : words					{ data->output = $1; }
     ;

words : words CHAR				{ $$ = concat($1, tostr($2)); }
      | words LETTER				{ $$ = concat($1, tostr($2)); }
      | words PIPE				{ $$ = concat($1, tostr($2)); }
      | words FORMAT				{ $$ = concat($1, tostr($2)); }
      | words cmd				{ $$ = concat($1, $2); }
      |						{ $$ = strdup(""); }
      ;

cmd : OCURLY name pipeline CCURLY		{ $$ = concat(strdup("PIPE-"), concat($2, $3)); }
    | OCURLY name FORMAT name CCURLY		{ $$ = concat(strdup("FMT-"), concat($2, $4)); }
    | OCURLY name CCURLY			{
							$$ = render_template(data->req, $2);
							free($2);
						}
    ;

pipeline : pipeline pipe			{ $$ = concat($1, $2); }
	 | pipe					{ $$ = $1; }
         ;

pipe : PIPE name				{ $$ = concat(strdup("P:"), $2); }
     ;

name : name LETTER				{ $$ = concat($1, tostr($2)); }
     | LETTER					{ $$ = tostr($1); }
     ;

%%
