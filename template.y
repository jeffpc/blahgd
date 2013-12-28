%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "vars.h"
#include "error.h"
#include "render.h"
#include "pipeline.h"
#include "utils.h"

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
	ASSERT(ret);

	ret[0] = c;
	ret[1] = '\0';

	return ret;
}

static int __expand_val(struct avl_node *node, void *data)
{
	struct vars *vars = data;
	struct val_item *val_item;

	val_item = container_of(node, struct val_item, tree);

	VAR_SET(vars, val_item->key.name, val_getref(val_item->val));

	return 0;
}

struct wrap_state {
	struct req *req;
	char *varname;
	char *tmpl;

	char *out;
};

static int __foreach_val(struct avl_node *node, void *arg)
{
	struct wrap_state *state = arg;
	struct req *req = state->req;
	struct val *val;
	char *ret;

	val = container_of(node, struct val_item, tree)->val;

	vars_scope_push(&req->vars);

	switch (val->type) {
		case VT_STR:
		case VT_INT:
			VAR_SET(&req->vars, state->varname, val_getref(val));
			break;
		case VT_NV:
			avl_for_each(&val->tree, __expand_val, &req->vars,
				     NULL, NULL);
			break;
		default:
			fprintf(stderr, "XXX %p\n", val);
			val_dump(val, 0);
			ASSERT(0);
			break;
	}

	ret = render_template(req, state->tmpl);

	state->out = concat(state->out, ret);

	vars_scope_pop(&req->vars);

	return !ret;
}

char *foreach(struct req *req, char *varname, char *tmpl)
{
	struct vars *vars = &req->vars;
	struct wrap_state state = {
		.req = req,
		.varname = varname,
		.tmpl = tmpl,
	};
	struct var *var;
	struct val *val;
	char *ret;

	var = var_lookup(vars, varname);
	if (!var)
		return xstrdup("");

	val = var->val;
	if (!val)
		return xstrdup("");

	if (val->type == VT_LIST) {
		state.out = NULL;
		avl_for_each(&val->tree, __foreach_val, &state, NULL, NULL);
		ret = state.out;
	} else {
		ret = NULL;
		LOG("%s called with '%s' which has type %d", __func__,
		    varname, val->type);
		ASSERT(0);
	}

	return ret;
}

static char *print_val(struct val *val)
{
	char buf[32];
	const char *tmp;

	tmp = NULL;

	switch (val->type) {
		case VT_STR:
			tmp = val->str;
			break;
		case VT_INT:
			snprintf(buf, sizeof(buf), "%lu", val->i);
			tmp = buf;
			break;
		case VT_LIST:
			tmp = "LIST";
			break;
		case VT_NV:
			tmp = "NV";
			break;
	}

	return xstrdup(tmp);
}

static char *print_var(struct var *var)
{
	return print_val(var->val);
}

static char *pipeline(struct req *req, char *var, struct pipeline *pipe)
{
	struct list_head line;
	struct val *val;
	struct var *v;
	struct pipeline *cur;
	struct pipeline *tmp;
	char *out;

	v = var_lookup(&req->vars, var);
	if (!v)
		return xstrdup("");

	/* wedge the sentinel list head into the pipeline */
	line.next = &pipe->pipe;
	line.prev = pipe->pipe.prev;
	pipe->pipe.prev->next = &line;
	pipe->pipe.prev = &line;

	val = val_getref(v->val);

	list_for_each_entry_safe(cur, tmp, &line, pipe)
		val = cur->stage->f(val);

	pipeline_destroy(&line);

	out = print_val(val);

	val_putref(val);

	return out;
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
      | words '|'				{ $$ = concat($1, xstrdup("|")); }
      | words '%'				{ $$ = concat($1, xstrdup("%")); }
      | words cmd				{ $$ = concat($1, $2); }
      |						{ $$ = xstrdup(""); }
      ;

cmd : '{' WORD pipeline '}'		{
						$$ = pipeline(data->req, $2, $3);
						free($2);
					}
    | '{' WORD '%' WORD '}'		{
						$$ = foreach(data->req, $2, $4);
						free($2);
						free($4);
					}
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

pipe : '|' WORD				{ $$ = pipestage($2); free($2); }
     ;

%%
