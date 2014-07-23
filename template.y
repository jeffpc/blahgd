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

char *foreach(struct req *req, char *varname, char *tmpl)
{
	nvpair_t *var;
	char *ret;

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
			snprintf(buf, sizeof(buf), "%lu", val->i);
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
			snprintf(buf, sizeof(buf), "%lu", pair2int(var));
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

static char *pipeline(struct req *req, char *varname, struct pipeline *line)
{
	struct pipestage *cur;
	struct val *val;
	nvpair_t *var;
	char *out;

	var = vars_lookup(&req->vars, varname);
	if (!var)
		return xstrdup("");

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
						nvpair_t *var;

						var = vars_lookup(&data->req->vars, $2);

						if (!var)
							$$ = render_template(data->req, $2);
						else
							$$ = print_var(var);

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

%%
