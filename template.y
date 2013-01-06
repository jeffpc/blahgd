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

%token <ptr> WORD
%token <c> CHAR

%type <ptr> words cmd pipeline pipe

%%

page : words					{ data->output = $1; }
     ;

words : words CHAR				{ $$ = concat($1, tostr($2)); }
      | words WORD				{ $$ = concat($1, $2); }
      | words '|'				{ $$ = concat($1, strdup("|")); }
      | words '%'				{ $$ = concat($1, strdup("%")); }
      | words cmd				{ $$ = concat($1, $2); }
      |						{ $$ = strdup(""); }
      ;

cmd : '{' WORD pipeline '}'		{ $$ = concat(strdup("PIPE-"), concat($2, $3)); }
    | '{' WORD '%' WORD '}'		{ $$ = concat(strdup("FMT-"), concat($2, $4)); }
    | '{' WORD '}'			{
						if (!is_var(&data->req->vars, $2))
							$$ = render_template(data->req, $2);
						else
							$$ = strdup("VAR");
						free($2);
					}
    ;

pipeline : pipeline pipe			{ $$ = concat($1, $2); }
	 | pipe					{ $$ = $1; }
         ;

pipe : '|' WORD				{ $$ = concat(strdup("P:"), $2); }
     ;

%%
