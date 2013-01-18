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
#include "error.h"
#include "render.h"
#include "pipeline.h"

#include "parse.h"

#define scanner data->scanner

extern int tmpl_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	LOG("Error: %s", e);
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

static void __foreach_vars(struct vars *vars, struct var_val *vv)
{
	int j, k;

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
}

char *foreach(struct req *req, char *var, char *tmpl)
{
	struct vars *vars = &req->vars;
	struct var *v;
	char *ret;
	char *tmp;
	int i;

	ret = strdup("");

	v = var_lookup(vars, var);
	if (!v)
		return ret;

	for (i = 0; i < VAR_MAX_ARRAY_SIZE; i++) {
		struct var_val *vv = &v->val[i];

		if (vv->type == VT_NIL)
			continue;

		vars_scope_push(vars);

		switch (vv->type) {
			case VT_NIL:
				assert(0);
				break;
			case VT_VARS:
				__foreach_vars(vars, vv);
				break;
			case VT_STR:
			case VT_INT:
				var_append(vars, var, vv);
				break;
		}

		tmp = render_template(req, tmpl);

		ret = concat(ret, tmp);

		vars_scope_pop(vars);
	}

	return ret;
}

static char *print_var_val(struct var_val *vv)
{
	char buf[32];
	char *tmp;

	tmp = NULL;

	switch (vv->type) {
		case VT_NIL:
			tmp = "NIL";
			break;
		case VT_STR:
			tmp = vv->str;
			break;
		case VT_INT:
			snprintf(buf, sizeof(buf), "%llu", vv->i);
			tmp = buf;
			break;
		case VT_VARS:
			tmp = "VARS";
			break;
	}

	return strdup(tmp);
}

static char *print_var(struct var *v)
{
	return print_var_val(&v->val[0]);
}

static char *pipeline(struct req *req, char *var, struct pipeline *pipe)
{
	struct list_head line;
	struct var_val *vv;
	struct var *v;
	struct pipeline *cur;
	struct pipeline *tmp;

	v = var_lookup(&req->vars, var);
	if (!v)
		return strdup("");

	/* wedge the sentinel list head into the pipeline */
	line.next = &pipe->pipe;
	line.prev = pipe->pipe.prev;
	pipe->pipe.prev->next = &line;
	pipe->pipe.prev = &line;

	vv = &v->val[0];

	list_for_each_entry_safe(cur, tmp, &line, pipe) {
		vv = cur->stage->f(vv);
	}

	return print_var_val(vv);
}
%}

%union {
	char c;
	char *ptr;
	struct pipeline *pipe;
};

%token <ptr> WORD
%token <c> CHAR

%type <ptr> words cmd
%type <pipe> pipeline pipe

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

cmd : '{' WORD pipeline '}'		{ $$ = pipeline(data->req, $2, $3); }
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

pipeline : pipeline pipe		{
						list_add_tail(&$2->pipe, &$1->pipe);
						$$ = $1;
					}
	 | pipe				{ $$ = $1; }
         ;

pipe : '|' WORD				{ $$ = pipestage($2); }
     ;

%%
