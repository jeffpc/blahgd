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

static char *condconcat(struct parser_output *data, char *a, char *b)
{
	if (!cond_value(data)) {
		free(b);
		return a;
	}

	return concat(a, b);
}

static char *__foreach_list(struct req *req, char *varname, char *tmpl,
                            struct val *val)
{
	avl_tree_t *tree;
	struct val_item *vi;
	char *ret;
	char *out;

	tree = &val->tree;

	out = xstrdup("");

	for (vi = avl_first(tree); vi; vi = AVL_NEXT(tree, vi)) {
		vars_scope_push(&req->vars);

		/* NOTE: we're reusing the arg */
		val = vi->val;

		switch (val->type) {
			case VT_STR:
			case VT_INT:
				VAR_SET(&req->vars, varname, val_getref(val));
				break;
			case VT_NV: {
				struct val_item *vi;

				for (vi = avl_first(&val->tree); vi;
				     vi = AVL_NEXT(&val->tree, vi))
					VAR_SET(&req->vars, vi->key.name,
					        val_getref(vi->val));
				break;
			}
			default:
				fprintf(stderr, "XXX %p\n", val);
				val_dump(val, 0);
				ASSERT(0);
				break;
		}

		ret = render_template(req, tmpl);

		out = concat(out, ret);

		vars_scope_pop(&req->vars);
	}

	return out;
}

static char *foreach(struct parser_output *data, struct req *req, char *varname,
                     char *tmpl)
{
	struct vars *vars = &req->vars;
	struct var *var;
	struct val *val;
	char *ret;

	if (!cond_value(data))
		return xstrdup("");

	var = var_lookup(vars, varname);
	if (!var)
		return xstrdup("");

	val = var->val;
	if (!val)
		return xstrdup("");

	if (val->type == VT_LIST) {
		ret = __foreach_list(req, varname, tmpl, val);
	} else {
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

static char *pipeline(struct parser_output *data, struct req *req, char *var,
                      struct pipeline *pipe)
{
	struct list_head line;
	struct val *val;
	struct var *v;
	struct pipeline *cur;
	struct pipeline *tmp;
	char *out;

	if (!cond_value(data))
		return xstrdup("");

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

static char *variable(struct parser_output *data, struct req *req, char *name)
{
	struct var *var;

	if (!cond_value(data))
		return xstrdup("");

	var = var_lookup(&data->req->vars, name);

	if (!var)
		return render_template(data->req, name);
	else
		return print_var(var);
}

static char *function(struct parser_output *data, struct req *req,
                      const char *fxn, void *args)
{
	fprintf(stderr, "%s(%p, .., %s)\n", __func__, data, fxn);

	if (!strcmp(fxn, "ifgt")) {
		cond_if(data, true /* FIXME */);
	} else if (!strcmp(fxn, "iflt")) {
		cond_if(data, true /* FIXME */);
	} else if (!strcmp(fxn, "ifeq")) {
		cond_if(data, true /* FIXME */);
	} else if (!strcmp(fxn, "endif")) {
		cond_endif(data);
	} else if (!strcmp(fxn, "else")) {
		cond_else(data);
	} else {
		LOG("unknown template function '%s'", fxn);
		ASSERT(0);
	}

	return xstrdup(NULL);
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
%type <ptr> args
 /* FIXME */

%%

page : words					{ data->output = $1; }
     ;

words : words CHAR				{ $$ = condconcat(data, $1, tostr($2)); }
      | words WORD				{ $$ = condconcat(data, $1, $2); }
      | words '|'				{ $$ = condconcat(data, $1, xstrdup("|")); }
      | words '%'				{ $$ = condconcat(data, $1, xstrdup("%")); }
      | words '('				{ $$ = condconcat(data, $1, xstrdup("(")); }
      | words ')'				{ $$ = condconcat(data, $1, xstrdup(")")); }
      | words ','				{ $$ = condconcat(data, $1, xstrdup(",")); }
      | words cmd				{ $$ = concat($1, $2); }
      |						{ $$ = xstrdup(""); }
      ;

cmd : '{' WORD pipeline '}'		{
						$$ = pipeline(data, data->req, $2, $3);
						free($2);
					}
    | '{' WORD '%' WORD '}'		{
						$$ = foreach(data, data->req, $2, $4);
						free($2);
						free($4);
					}
    | '{' WORD '(' args ')' '}'		{
						$$ = function(data, data->req, $2, $4);
						free($2);
					}
    | '{' WORD '(' ')' '}'		{
						$$ = function(data, data->req, $2, NULL);
						free($2);
					}
    | '{' WORD '}'			{
						$$ = variable(data, data->req, $2);
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

args : args ',' arg			{ $$ = NULL; }
     | arg				{ $$ = NULL; }
     ;

arg : WORD
    ;

%%
