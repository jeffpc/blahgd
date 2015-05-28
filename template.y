%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "iter.h"
#include "vars.h"
#include "error.h"
#include "render.h"
#include "pipeline.h"
#include "utils.h"
#include "val.h"

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

static char *__foreach_nv(struct req *req, nvpair_t *var, char *tmpl)
{
	nvlist_t **items;
	uint_t nitems;
	uint_t i;
	char *out;
	int ret;

	out = NULL;

	ret = nvpair_value_nvlist_array(var, &items, &nitems);
	ASSERT0(ret);

	for (i = 0; i < nitems; i++) {
		vars_scope_push(&req->vars);

		vars_merge(&req->vars, items[i]);

		out = concat(out, render_template(req, tmpl));

		vars_scope_pop(&req->vars);
	}

	return out;
}

static char *__foreach_str(struct req *req, nvpair_t *var, char *tmpl)
{
	uint_t nitems;
	uint_t i;
	char **items;
	char *out;
	int ret;

	out = NULL;

	ret = nvpair_value_string_array(var, &items, &nitems);
	ASSERT0(ret);

	for (i = 0; i < nitems; i++) {
		vars_scope_push(&req->vars);

		vars_set_str(&req->vars, nvpair_name(var), items[i]);

		out = concat(out, render_template(req, tmpl));

		vars_scope_pop(&req->vars);
	}

	return out;
}

static char *foreach(struct parser_output *data, struct req *req, char *varname,
                     char *tmpl)
{
	nvpair_t *var;
	char *ret;

	if (!cond_value(data))
		return xstrdup("");

	var = vars_lookup(&req->vars, varname);
	if (!var)
		return xstrdup("");

	if (nvpair_type(var) == DATA_TYPE_NVLIST_ARRAY) {
		ret = __foreach_nv(req, var, tmpl);
	} else if (nvpair_type(var) == DATA_TYPE_STRING_ARRAY) {
		ret = __foreach_str(req, var, tmpl);
	} else {
		//vars_dump(&req->vars);
		LOG("%s called with '%s' which has type %d", __func__,
		    varname, nvpair_type(var));
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
			snprintf(buf, sizeof(buf), "%"PRIu64, val->i);
			tmp = buf;
			break;
	}

	return xstrdup(tmp);
}

static char *print_var(nvpair_t *var)
{
	char buf[32];
	const char *tmp;

	tmp = NULL;

	switch (nvpair_type(var)) {
		case DATA_TYPE_STRING:
			tmp = pair2str(var);
			break;
		case DATA_TYPE_UINT64:
			snprintf(buf, sizeof(buf), "%"PRIu64, pair2int(var));
			tmp = buf;
			break;
		default:
			LOG("%s called with '%s' which has type %d", __func__,
			    nvpair_name(var), nvpair_type(var));
			ASSERT(0);
			break;
	}

	return xstrdup(tmp);
}

static char *pipeline(struct parser_output *data, struct req *req,
		      char *varname, struct pipeline *line)
{
	struct pipestage *cur;
	struct val *val;
	nvpair_t *var;
	char *out;

	if (!cond_value(data)) {
		pipeline_destroy(line);
		return xstrdup("");
	}

	var = vars_lookup(&req->vars, varname);
	if (!var) {
		pipeline_destroy(line);
		return xstrdup("");
	}

	switch (nvpair_type(var)) {
		case DATA_TYPE_STRING:
			val = VAL_ALLOC_STR(xstrdup(pair2str(var)));
			break;
		case DATA_TYPE_UINT64:
			val = VAL_ALLOC_INT(pair2int(var));
			break;
		default:
			vars_dump(&req->vars);
			LOG("%s called with '%s' which has type %d", __func__,
			    varname, nvpair_type(var));
			ASSERT(0);
			break;
	}

	list_for_each(cur, &line->pipe)
		val = cur->stage->f(val);

	pipeline_destroy(line);

	out = print_val(val);

	val_putref(val);

	return out;
}

static char *variable(struct parser_output *data, struct req *req, char *name)
{
	nvpair_t *var;

	if (!cond_value(data))
		return xstrdup("");

	var = vars_lookup(&req->vars, name);

	if (!var)
		return render_template(req, name);
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
	struct pipestage *pipestage;
	struct pipeline *pipeline;
};

%token <ptr> WORD
%token <c> CHAR

%type <ptr> words cmd
%type <pipeline> pipeline
%type <pipestage> pipe
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
						pipeline_append($1, $2);
						$$ = $1;
					}
	 | pipe				{ $$ = pipestage_to_pipeline($1); }
         ;

pipe : '|' WORD				{ $$ = pipestage_alloc($2); free($2); }
     ;

args : args ',' arg			{ $$ = NULL; }
     | arg				{ $$ = NULL; }
     ;

arg : WORD
    ;

%%
