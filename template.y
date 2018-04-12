/*
 * Copyright (c) 2012-2018 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/error.h>
#include <jeffpc/val.h>

#include "config.h"
#include "iter.h"
#include "vars.h"
#include "render.h"
#include "pipeline.h"
#include "utils.h"
#include "debug.h"

#include "parse.h"

#define scanner data->scanner

extern int tmpl_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	DBG("Error: %s", e);
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

static char *__foreach(struct req *req, const struct nvpair *var, char *tmpl)
{
	struct val **items;
	size_t nitems;
	size_t i;
	char *out;
	int ret;

	out = NULL;

	ret = nvpair_value_array(var, &items, &nitems);
	ASSERT0(ret);

	for (i = 0; i < nitems; i++) {
		vars_scope_push(&req->vars);

		switch (items[i]->type) {
			case VT_NVL:
				vars_merge(&req->vars,
					   val_cast_to_nvl(items[i]));
				break;
			case VT_STR:
				vars_set_str(&req->vars, nvpair_name(var),
					     val_getref_str(items[i]));
				break;
			default:
				//vars_dump(&req->vars);
				panic("%s called with '%s' which has type %d",
				      __func__, nvpair_name(var),
				      nvpair_type(var));
		}

		out = concat(out, render_template(req, tmpl));

		vars_scope_pop(&req->vars);
	}

	return out;
}

static char *foreach(struct parser_output *data, struct req *req, char *varname,
                     char *tmpl)
{
	const struct nvpair *var;

	if (!cond_value(data))
		return xstrdup("");

	var = vars_lookup(&req->vars, varname);
	if (!var)
		return xstrdup("");

	return __foreach(req, var, tmpl);
}

static char *print_val(struct val *val)
{
	char buf[32];
	const char *tmp;

	tmp = NULL;

	switch (val->type) {
		case VT_STR:
			tmp = str_cstr(val_cast_to_str(val));
			break;
		case VT_INT:
			snprintf(buf, sizeof(buf), "%"PRIu64, val->i);
			tmp = buf;
			break;
		case VT_BLOB:
		case VT_NULL:
		case VT_SYM:
		case VT_CONS:
		case VT_BOOL:
		case VT_CHAR:
		case VT_ARRAY:
		case VT_NVL:
			panic("%s called with value of type %d", __func__,
			      val->type);
	}

	return xstrdup(tmp);
}

static char *print_var(const struct nvpair *var)
{
	struct str *str;
	char buf[32];
	char *ret;

	switch (nvpair_type(var)) {
		case VT_STR:
			str = nvpair_value_str(var);
			ret = xstrdup(str_cstr(str));
			str_putref(str);
			break;
		case VT_INT:
			snprintf(buf, sizeof(buf), "%"PRIu64, pair2int(var));
			ret = xstrdup(buf);
			break;
		default:
			panic("%s called with '%s' which has type %d", __func__,
			      nvpair_name(var), nvpair_type(var));
			break;
	}

	return ret;
}

static char *pipeline(struct parser_output *data, struct req *req,
		      char *varname, struct pipeline *line)
{
	const struct nvpair *var;
	struct pipestage *cur;
	struct val *val;
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
		case VT_STR:
			val = str_cast_to_val(nvpair_value_str(var));
			break;
		case VT_INT:
			val = VAL_ALLOC_INT(pair2int(var));
			break;
		default:
			vars_dump(&req->vars);
			panic("%s called with '%s' which has type %d", __func__,
			      varname, nvpair_type(var));
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
	const struct nvpair *var;

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
	const struct nvpair *var;
	uint64_t val;

	if (!str2u64(arg, &val))
		return val;

	var = vars_lookup(&req->vars, arg);
	if (!var)
		return 0;

	switch (nvpair_type(var)) {
		case VT_INT:
			return pair2int(var);
		default:
			panic("unexpected nvpair type: %d",
			      nvpair_type(var));
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

static char *__function_ifset(struct parser_output *data, struct req *req,
			      const char *arg)
{
	const struct nvpair *var;

	var = vars_lookup(&req->vars, arg);

	cond_if(data, var != NULL);

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
	} else if (!strcmp(fxn, "ifset")) {
		return __function_ifset(data, req, sa1);
	} else if (!strcmp(fxn, "endif")) {
		cond_endif(data);
	} else if (!strcmp(fxn, "else")) {
		cond_else(data);
	} else {
		panic("unknown template function '%s'", fxn);
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
						free($4);
						free($6);
					}
    | '{' WORD '(' WORD ')' '}'		{
						$$ = function(data, data->req, $2, $4, NULL);
						free($2);
						free($4);
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
