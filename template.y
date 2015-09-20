/*
 * Copyright (c) 2012-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
		case VT_CSTR:
			tmp = val->cstr;
			break;
		case VT_INT:
			snprintf(buf, sizeof(buf), "%"PRIu64, val->i);
			tmp = buf;
			break;
		case VT_SYM:
		case VT_CONS:
		case VT_BOOL:
			LOG("%s called with value of type %d", __func__,
			    val->type);
			ASSERT(0);
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
			val = VAL_ALLOC_CSTR(xstrdup(pair2str(var)));
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

	list_for_each(&line->pipe, cur)
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

enum if_fxns {
	IFFXN_GT,
	IFFXN_LT,
	IFFXN_EQ,
};

static uint64_t __function_get_arg(struct req *req, const char *arg)
{
	nvpair_t *var;
	uint64_t val;

	if (!str2u64(arg, &val))
		return val;

	var = vars_lookup(&req->vars, arg);
	if (!var)
		return 0;

	switch (nvpair_type(var)) {
		case DATA_TYPE_UINT64:
			return pair2int(var);
		default:
			ASSERT(0);
	}
}

static char *__function(struct parser_output *data, struct req *req,
			enum if_fxns fxn, const char *sa1,
			const char *sa2)
{
	uint64_t ia1, ia2;		/* int value of saX */
	bool result = true;

	ia1 = __function_get_arg(req, sa1);
	ia2 = __function_get_arg(req, sa2);

	switch (fxn) {
		case IFFXN_GT:
			result = ia1 > ia2;
			break;
		case IFFXN_LT:
			result = ia1 < ia2;
			break;
		case IFFXN_EQ:
			result = ia1 == ia2;
			break;
	}

	cond_if(data, result);

	return xstrdup(NULL);
}

static char *function(struct parser_output *data, struct req *req,
                      const char *fxn, const char *sa1, const char *sa2)
{
	if (!strcmp(fxn, "ifgt")) {
		return __function(data, req, IFFXN_GT, sa1, sa2);
	} else if (!strcmp(fxn, "iflt")) {
		return __function(data, req, IFFXN_LT, sa1, sa2);
	} else if (!strcmp(fxn, "ifeq")) {
		return __function(data, req, IFFXN_EQ, sa1, sa2);
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
    | '{' WORD '(' WORD ',' WORD ')' '}'{
						$$ = function(data, data->req, $2, $4, $6);
						free($2);
					}
    | '{' WORD '(' WORD ')' '}'		{
						$$ = function(data, data->req, $2, $4, NULL);
						free($2);
					}
    | '{' WORD '(' ')' '}'		{
						$$ = function(data, data->req, $2, NULL, NULL);
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

%%
