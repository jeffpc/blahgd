%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "vars.h"

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
	struct vars *vars = &req->vars;
	struct var *v;
	int i, j, k;
	char *ret;
	char *tmp;

	ret = strdup("");

	v = var_lookup(&req->vars, var);
	assert(v);

	for (i = 0; i < VAR_MAX_ARRAY_SIZE; i++) {
		struct var_val *vv = &v->val[i];

		if (vv->type == VT_NIL)
			continue;

		assert(vv->type == VT_VARS);

		vars_scope_push(vars);

		for (j = 0; j < VAR_MAX_VARS_SIZE; j++) {
			if (!vv->vars[j])
				continue;

			for (k = 0; k < VAR_MAX_ARRAY_SIZE; k++) {
				if (vv->vars[j]->val[k].type == VT_NIL)
					continue;

				var_append(vars, vv->vars[j]->name,
					   &vv->vars[j]->val[k]);
			}
		}

		fprintf(stderr, "%s (%d)\n", __func__, i);
		vars_dump(vars);

		tmp = render_template(req, tmpl);

		ret = concat(ret, tmp);

		vars_scope_pop(vars);
	}

	return ret;
}

static char *print_var(struct var *v)
{
	char buf[32];
	char *ret;
	char *tmp;

	ret = strdup("");
	assert(ret);

	tmp = NULL;

	switch (v->val[0].type) {
		case VT_NIL:
			tmp = "NIL";
			break;
		case VT_STR:
			tmp = v->val[0].str;
			break;
		case VT_INT:
			snprintf(buf, sizeof(buf), "%llu", v->val[0].i);
			tmp = buf;
			break;
		case VT_VARS:
			tmp = "VARS";
			break;
	}

	ret = concat(ret, strdup(tmp));

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
    | '{' WORD '%' WORD '}'		{ $$ = foreach(data->req, $2, $4); }
    | '{' WORD '}'			{
						struct var *var;

						var = var_lookup(&data->req->vars, $2);

						if (!var)
							$$ = render_template(data->req, $2);
						else
							$$ = print_var(var);

						free($2);
					}
    ;

pipeline : pipeline pipe			{ $$ = concat($1, $2); }
	 | pipe					{ $$ = $1; }
         ;

pipe : '|' WORD				{ $$ = concat(strdup("P:"), $2); }
     ;

%%
