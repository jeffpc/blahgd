/*
 * Copyright (c) 2011-2019 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <jeffpc/error.h>
#include <jeffpc/types.h>

#include "config.h"
#include "utils.h"
#include "mangle.h"
#include "listing.h"
#include "post_fmt3_cmds.h"
#include "debug.h"

#include "parse.h"

#define scanner data->scanner

extern int fmt3_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	DBG("Error: %s", e);
}

void fmt3_error2(char *e, char *yytext)
{
	DBG("Error: %s (%s)", e, yytext);
}

static const char *dashes[4] = {
	[0] = NULL,
	[1] = "-",
	[2] = "&ndash;",
	[3] = "&mdash;",
};

static const char *oquotes[3] = {
	[0] = NULL,
	[1] = "&lsquo;",
	[2] = "&ldquo;",
};

static const char *cquotes[3] = {
	[0] = NULL,
	[1] = "&rsquo;",
	[2] = "&rdquo;",
};

static struct str *__char_cvt(struct str *val, const char **outstr, size_t nouts)
{
	size_t len;

	len = str_len(val);

	ASSERT3U(len, <=, nouts);

	str_putref(val);

	return STR_DUP(outstr[len]);
}

#define dash(v)		__char_cvt((v), dashes, ARRAY_LEN(dashes))
#define oquote(v)	__char_cvt((v), oquotes, ARRAY_LEN(oquotes))
#define cquote(v)	__char_cvt((v), cquotes, ARRAY_LEN(cquotes))

static struct str *special_char(struct str *val)
{
	struct str *ret;

	ASSERT3U(str_len(val), ==, 1);

	switch (str_cstr(val)[0]) {
		case '"': ret = STATIC_STR("&quot;"); break;
		case '<': ret = STATIC_STR("&lt;"); break;
		case '>': ret = STATIC_STR("&gt;"); break;
		default:
			panic("%s given an unexpected character '%c'",
			      __func__, str_cstr(val)[0]);
	}

	str_putref(val);

	return ret;
}

static void special_cmd(struct parser_output *data, struct str **var,
			struct str *value, bool photo)
{
	str_putref(*var);

	if (photo)
		*var = str_cat(3, str_getref(config.photo_base_url),
			       STATIC_STR("/"),
			       value);
	else
		*var = value;
}

static void special_cmd_list(struct parser_output *data, struct val **var,
			     struct str *value)
{
	/* extend the list */
	*var = VAL_ALLOC_CONS(str_cast_to_val(value), *var);
}

static struct str *math_apply(struct str *op, struct str *a, struct str *b)
{
	str_getref(op);
	if (b)
		return str_cat(9,
			       STATIC_STR("<"), op, STATIC_STR("><mrow>"),
			       a, STATIC_STR("</mrow><mrow>"), b,
			       STATIC_STR("</mrow></"), op, STATIC_STR(">"));
	else
		return str_cat(7,
			       STATIC_STR("<"), op, STATIC_STR("><mrow>"),
			       a,
			       STATIC_STR("</mrow></"), op, STATIC_STR(">"));
}

%}

%union {
	struct str *ptr;
};

/* generic tokens */
%token <ptr> WSPACE
%token <ptr> DASH OQUOT CQUOT SCHAR CHAR
%token <ptr> UTF8CHAR WORD NUMBER
%token PERCENT ELLIPSIS
%token PAREND

/* math specific tokens */
%token MATHSTART MATHEND
%token MATHFRAC MATHSQRT
%token MATHPROPTO

/* verbose & listing environment */
%token <ptr> VERBTEXT
%token VERBSTART VERBEND DOLLAR
%token LISTSTART LISTEND
%token TITLESTART TAGSTART PUBSTART TWITTERIMGSTART
%token TWITTERPHOTOSTART
%token SPECIALCMDEND

%type <ptr> paragraphs paragraph thing cmd cmdarg optcmdarg math mexpr
%type <ptr> verb

%left '=' '<' '>'
%left '*' '/'
%left '+' '-'
%left '_' '^'
%left '~'

%%

post : paragraphs PAREND		{ data->stroutput = $1; }
     | paragraphs			{ data->stroutput = $1; }
     | PAREND				{ data->stroutput = str_empty_string(); }
     ;

paragraphs : paragraphs PAREND paragraph	{ $$ = str_cat(4, $1, STATIC_STR("<p>"), $3, STATIC_STR("</p>\n")); }
	   | paragraph				{ $$ = str_cat(3, STATIC_STR("<p>"), $1, STATIC_STR("</p>\n")); }
	   ;

paragraph : paragraph thing		{ $$ = str_cat(2, $1, $2); }
          | thing			{ $$ = $1; }
          ;

thing : WORD				{ $$ = $1; }
      | UTF8CHAR			{ $$ = $1; }
      | '\n'				{ $$ = data->texttt_nesting ? STATIC_STR("\n") : STATIC_STR(" "); }
      | WSPACE				{ $$ = $1; }
      | DASH				{ $$ = dash($1); }
      | OQUOT				{ $$ = oquote($1); }
      | CQUOT				{ $$ = cquote($1); }
      | CHAR				{ $$ = $1; }
      | SCHAR				{ $$ = special_char($1); }
      | ELLIPSIS			{ $$ = STATIC_STR("&hellip;"); }
      | '~'				{ $$ = STATIC_STR("&nbsp;"); }
      | '&'				{ $$ = STATIC_STR("</td><td>"); }
      | DOLLAR				{ $$ = STATIC_STR("$"); }
      | PERCENT				{ $$ = STATIC_STR("%"); }
      | '\\' cmd			{ $$ = $2; }
      | MATHSTART math MATHEND		{ $$ = str_cat(3, STATIC_STR("<math>"), $2, STATIC_STR("</math>")); }
      | VERBSTART verb VERBEND		{ $$ = str_cat(3, STATIC_STR("</p>"), $2, STATIC_STR("<p>")); }
      | LISTSTART verb LISTEND		{ $$ = str_cat(3, STATIC_STR("</p><pre>"),
						       listing_str($2),
						       STATIC_STR("</pre><p>")); }
      | TITLESTART verb SPECIALCMDEND	{ $$ = NULL; special_cmd(data, &data->sc_title, $2, false); }
      | PUBSTART verb SPECIALCMDEND	{ $$ = NULL; special_cmd(data, &data->sc_pub, $2, false); }
      | TAGSTART verb SPECIALCMDEND	{ $$ = NULL; special_cmd_list(data, &data->sc_tags, $2); }
      | TWITTERIMGSTART verb SPECIALCMDEND
					{ $$ = NULL; special_cmd(data, &data->sc_twitter_img, $2, false); }
      | TWITTERPHOTOSTART verb SPECIALCMDEND
					{ $$ = NULL; special_cmd(data, &data->sc_twitter_img, $2, true); }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd(data, $1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd(data, $1, $2, NULL); }
    | WORD			{ $$ = process_cmd(data, $1, NULL, NULL); }
    | '\\'			{ $$ = STATIC_STR("<br/>"); }
    | '{'			{ $$ = STATIC_STR("{"); }
    | '}'			{ $$ = STATIC_STR("}"); }
    | '['			{ $$ = STATIC_STR("["); }
    | ']'			{ $$ = STATIC_STR("]"); }
    | '&'			{ $$ = STATIC_STR("&amp;"); }
    | '_'			{ $$ = STATIC_STR("_"); }
    | '^'			{ $$ = STATIC_STR("^"); }
    | '~'			{ $$ = STATIC_STR("~"); }
    ;

optcmdarg : '[' paragraph ']'	{ $$ = $2; }
          ;

cmdarg : '{' paragraph '}'	{ $$ = $2; }
       ;

verb : verb VERBTEXT			{ $$ = str_cat(2, $1, $2); }
     | VERBTEXT				{ $$ = $1; }

math : math mexpr			{ $$ = str_cat(2, $1, $2); }
     | mexpr				{ $$ = $1; }
     ;

mexpr : mexpr '+' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>+</mo>"), $3); }
      | mexpr '-' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>-</mo>"), $3); }
      | mexpr '*' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>*</mo>"), $3); }
      | mexpr '/' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>/</mo>"), $3); }
      | mexpr '^' mexpr			{ $$ = math_apply(STATIC_STR("msup"), $1, $3); }
      | mexpr '_' mexpr			{ $$ = math_apply(STATIC_STR("msub"), $1, $3); }
      | mexpr '=' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>=</mo>"), $3); }
      | mexpr '<' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>&lt;</mo>"), $3); }
      | mexpr '>' mexpr			{ $$ = str_cat(3, $1, STATIC_STR("<mo>&gt;</mo>"), $3); }
      | mexpr '~' mexpr			{ $$ = str_cat(2, $1, $3); }
      | '(' mexpr ')'			{ $$ = str_cat(3,
						       STATIC_STR("<mrow><mo>(</mo><mrow>"),
						       $2,
						       STATIC_STR("</mrow><mo>)</mo></mrow>")); }
      | '{' mexpr '}'			{ $$ = $2; }
      | '\\' MATHFRAC '{' mexpr '}' '{' mexpr '}'
					{ $$ = math_apply(STATIC_STR("mfrac"), $4, $7); }
      | '\\' MATHSQRT '{' mexpr '}'	{ $$ = math_apply(STATIC_STR("msqrt"), $4, NULL); }
      | '\\' MATHPROPTO			{ $$ = STATIC_STR("<mo>&prop;</mo>"); }
      | NUMBER				{ $$ = str_cat(3, STATIC_STR("<mn>"), $1, STATIC_STR("</mn>")); }
      | WORD				{ $$ = str_cat(3, STATIC_STR("<mi>"), $1, STATIC_STR("</mi>")); }
      | MATHFRAC			{ $$ = STATIC_STR("<mi>frac</mi>"); }
      | MATHSQRT			{ $$ = STATIC_STR("<mi>sqrt</mi>"); }
      | MATHPROPTO			{ $$ = STATIC_STR("<mi>propto</mi>"); }
      ;

%%
