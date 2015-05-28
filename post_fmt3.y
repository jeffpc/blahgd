%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "error.h"
#include "utils.h"
#include "mangle.h"
#include "listing.h"
#include "post_fmt3_cmds.h"
#include "str.h"
#include "math.h"

#include "parse.h"

#define scanner data->scanner

extern int fmt3_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	LOG("Error: %s", e);
}

void fmt3_error2(char *e, char *yytext)
{
	LOG("Error: %s (%s)", e, yytext);
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

	len = strlen(val->str);

	ASSERT3U(len, <=, nouts);

	str_putref(val);

	return STR_DUP(outstr[len]);
}

#define dash(v)		__char_cvt((v), dashes, ARRAY_LEN(dashes))
#define oquote(v)	__char_cvt((v), oquotes, ARRAY_LEN(oquotes))
#define cquote(v)	__char_cvt((v), cquotes, ARRAY_LEN(cquotes))

static struct str *special_char(struct str *val)
{
	char *ret;

	ASSERT3U(strlen(val->str), ==, 1);

	switch (val->str[0]) {
		case '"': ret = "&quot;"; break;
		case '<': ret = "&lt;"; break;
		case '>': ret = "&gt;"; break;
		default:
			return val;
	}

	str_putref(val);

	return STR_DUP(ret);
}

%}

%union {
	struct str *ptr;
};

/* generic tokens */
%token <ptr> WSPACE
%token <ptr> DASH OQUOT CQUOT SCHAR
%token <ptr> UTF8FIRST3 UTF8FIRST2 UTF8REST WORD
%token SLASH
%token PIPE
%token OCURLY CCURLY OBRACE CBRACE
%token USCORE ASTERISK
%token BSLASH PERCENT AMP TILDE ELLIPSIS
%token PAREND NLINE

/* math specific tokens */
%token <ptr> PLUS MINUS EQLTGT CARRET
%token OPAREN CPAREN
%token MATHSTART MATHEND

/* verbose & listing environment */
%token <ptr> VERBTEXT
%token VERBSTART VERBEND DOLLAR
%token LISTSTART LISTEND

%type <ptr> paragraphs paragraph thing cmd cmdarg optcmdarg math mexpr
%type <ptr> verb

%left USCORE CARRET
%left EQLTGT
%left PLUS MINUS
%left ASTERISK SLASH

%%

post : paragraphs PAREND		{ data->stroutput = $1; }
     | paragraphs			{ data->stroutput = $1; }
     | PAREND				{ data->stroutput = STR_DUP(""); }
     ;

paragraphs : paragraphs PAREND paragraph	{ $$ = str_cat4($1, STR_DUP("<p>"), $3, STR_DUP("</p>\n")); }
	   | paragraph				{ $$ = str_cat3(STR_DUP("<p>"), $1, STR_DUP("</p>\n")); }
	   ;

paragraph : paragraph thing		{ $$ = str_cat($1, $2); }
          | thing			{ $$ = $1; }
          ;

thing : WORD				{ $$ = $1; }
      | UTF8FIRST2 UTF8REST		{ $$ = str_cat($1, $2); }
      | UTF8FIRST3 UTF8REST UTF8REST	{ $$ = str_cat3($1, $2, $3); }
      | NLINE				{ $$ = STR_DUP(data->post->texttt_nesting ? "\n" : " "); }
      | WSPACE				{ $$ = $1; }
      | PIPE				{ $$ = STR_DUP("|"); }
      | ASTERISK			{ $$ = STR_DUP("*"); }
      | SLASH				{ $$ = STR_DUP("/"); }
      | DASH				{ $$ = dash($1); }
      | OQUOT				{ $$ = oquote($1); }
      | CQUOT				{ $$ = cquote($1); }
      | SCHAR				{ $$ = special_char($1); }
      | ELLIPSIS			{ $$ = STR_DUP("&hellip;"); }
      | TILDE				{ $$ = STR_DUP("&nbsp;"); }
      | AMP				{ $$ = STR_DUP("</td><td>"); }
      | DOLLAR				{ $$ = STR_DUP("$"); }
      | PERCENT				{ $$ = STR_DUP("%"); }
      | BSLASH cmd			{ $$ = $2; }
      | MATHSTART math MATHEND		{ $$ = render_math($2); }
      | VERBSTART verb VERBEND		{ $$ = str_cat3(STR_DUP("</p><p>"), $2, STR_DUP("</p><p>")); }
      | LISTSTART verb LISTEND		{ $$ = str_cat3(STR_DUP("</p><pre>"),
							 listing_str($2),
							 STR_DUP("</pre><p>")); }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd(data->post, $1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd(data->post, $1, $2, NULL); }
    | WORD			{ $$ = process_cmd(data->post, $1, NULL, NULL); }
    | BSLASH			{ $$ = STR_DUP("<br/>"); }
    | OCURLY			{ $$ = STR_DUP("{"); }
    | CCURLY			{ $$ = STR_DUP("}"); }
    | OBRACE			{ $$ = STR_DUP("["); }
    | CBRACE			{ $$ = STR_DUP("]"); }
    | AMP			{ $$ = STR_DUP("&amp;"); }
    | USCORE			{ $$ = STR_DUP("_"); }
    | TILDE			{ $$ = STR_DUP("~"); }
    ;

optcmdarg : OBRACE paragraph CBRACE	{ $$ = $2; }
          ;

cmdarg : OCURLY paragraph CCURLY	{ $$ = $2; }
       ;

verb : verb VERBTEXT			{ $$ = str_cat($1, $2); }
     | VERBTEXT				{ $$ = $1; }

math : math mexpr			{ $$ = str_cat($1, $2); }
     | mexpr				{ $$ = $1; }
     ;

mexpr : WORD				{ $$ = $1; }
      | WSPACE				{ $$ = $1; }
      | SCHAR				{ $$ = $1; }
      | mexpr EQLTGT mexpr 		{ $$ = str_cat3($1, $2, $3); }
      | mexpr USCORE mexpr 		{ $$ = str_cat3($1, STR_DUP("_"), $3); }
      | mexpr CARRET mexpr 		{ $$ = str_cat3($1, $2, $3); }
      | mexpr PLUS mexpr 		{ $$ = str_cat3($1, $2, $3); }
      | mexpr MINUS mexpr 		{ $$ = str_cat3($1, $2, $3); }
      | mexpr ASTERISK mexpr	 	{ $$ = str_cat3($1, STR_DUP("*"), $3); }
      | mexpr SLASH mexpr	 	{ $$ = str_cat3($1, STR_DUP("/"), $3); }
      | BSLASH WORD			{ $$ = str_cat(STR_DUP("\\"), $2); }
      | BSLASH USCORE			{ $$ = STR_DUP("\\_"); }
      | OPAREN math CPAREN		{ $$ = str_cat3(STR_DUP("("), $2, STR_DUP(")")); }
      | OCURLY math CCURLY		{ $$ = str_cat3(STR_DUP("{"), $2, STR_DUP("}")); }
      ;

%%
