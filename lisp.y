/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
#include "error.h"
#include "lisp.h"

#include "parse.h"

#define scanner data->scanner

extern int lisp_reader_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	LOG("Error: %s", e);
}
%}

%union {
	char *s;
	uint64_t i;
	bool b;
	struct val *lv;
};

%token <s> SYMBOL STRING
%token <i> NUMBER
%token <b> BOOL

%type <lv> document tok list toklist

%%
document : '\'' tok		{ data->valoutput = $2; }
	 ;

tok : SYMBOL			{ $$ = VAL_ALLOC_SYM(STR_DUP($1)); }
    | STRING			{ $$ = VAL_ALLOC_CSTR($1); }
    | NUMBER			{ $$ = VAL_ALLOC_INT($1); }
    | BOOL			{ $$ = VAL_ALLOC_BOOL($1); }
    | list			{ $$ = $1; }
    ;

toklist : tok toklist		{ $$ = VAL_ALLOC_CONS($1, $2); }
	| tok			{ $$ = VAL_ALLOC_CONS($1, NULL); }
	;

list : '(' ')'			{ $$ = NULL; }
     | '(' toklist ')'		{ $$ = $2; }
     | '(' tok '.' tok ')'	{ $$ = VAL_ALLOC_CONS($2, $4); }
     ;

%%
