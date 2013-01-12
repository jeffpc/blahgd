%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "listing.h"

#include "parse.h"

#define scanner data->scanner

extern int fmt3_lex(void *, void *);

void yyerror(void *scan, char *e)
{
	fprintf(stderr, "Error: %s\n", e);
}

void fmt3_error2(char *e, char *yytext)
{
	fprintf(stderr, "Error: %s (%s)\n", e, yytext);
}

static char *concat(char *a, char *b)
{
	char *ret;

	ret = malloc(strlen(a) + strlen(b) + 1);
	assert(ret);

	strcpy(ret, a);
	strcat(ret, b);

	return ret;
}

static char *concat4(char *a, char *b, char *c, char *d)
{
	char *ret;

	ret = malloc(strlen(a) + strlen(b) + strlen(c) + strlen(d) + 1);
	assert(ret);

	strcpy(ret, a);
	strcat(ret, b);
	strcat(ret, c);
	strcat(ret, d);

	return ret;
}

static char *concat5(char *a, char *b, char *c, char *d, char *e)
{
	char *ret;

	ret = malloc(strlen(a) + strlen(b) + strlen(c) + strlen(d) + strlen(e) + 1);
	assert(ret);

	strcpy(ret, a);
	strcat(ret, b);
	strcat(ret, c);
	strcat(ret, d);
	strcat(ret, e);

	return ret;
}

static char *__listing(struct post *post, char *txt, char *opt)
{
	return concat4("</p><pre>", "", listing(post, txt), "</pre><p>");
}

static char *process_cmd(struct post *post, char *cmd, char *txt, char *opt)
{
	if (!strcmp(cmd, "link"))
		return concat5("<a href=\"", txt, "\">", opt ? opt : txt, "</a>");

	if (!strcmp(cmd, "photolink"))
		return concat5("<a href=\"" PHOTO_BASE_URL "/", txt, "\">", opt ? opt : txt, "</a>");

	if (!strcmp(cmd, "img"))
		return concat5("<img src=\"", txt, "\" alt=\"", opt ? opt : "", "\" />");

	if (!strcmp(cmd, "photo"))
		return concat5("<img src=\"" PHOTO_BASE_URL "/", txt, "\" alt=\"", opt ? opt : "", "\" />");

	if (!strcmp(cmd, "emph")) {
		assert(!opt);
		return concat4("<em>", txt, "</em>", "");
	}

	if (!strcmp(cmd, "texttt")) {
		assert(!opt);
		return concat4("<tt>", txt, "</tt>", "");
	}

	if (!strcmp(cmd, "textbf")) {
		assert(!opt);
		return concat4("<strong>", txt, "</strong>", "");
	}

	if (!strcmp(cmd, "listing"))
		return __listing(post, txt, opt);

	if (!strcmp(cmd, "item")) {
		// FIXME: we should keep track of what commands we've
		// encountered and then decide if <li> is the right tag to
		// use
		if (!opt)
			return concat4("<li>", txt, "</li>", "");
		return concat5("<dt>", opt, "</dt><dd>", txt, "</dd>");
	}

	if (!strcmp(cmd, "begin") || !strcmp(cmd, "end")) {
		int begin = !strcmp(cmd, "begin");

		if (!strcmp(txt, "enumerate")) {
			assert(!opt);
			return strdup(begin ? "</p><ol>" : "</ol><p>");
		}

		if (!strcmp(txt, "itemize")) {
			assert(!opt);
			return strdup(begin ? "</p><ul>" : "</ul><p>");
		}

		if (!strcmp(txt, "description")) {
			assert(!opt);
			return strdup(begin ? "</p><dl>" : "</dl><p>");
		}

		if (!strcmp(txt, "quote")) {
			assert(!opt);
			return strdup(begin ? "</p><blockquote><p>" :
					      "</p></blockquote><p>");
		}

		return concat4("[INVAL ENVIRON", txt, "]", "");
	}

	if (!strcmp(cmd, "abbrev"))
		return concat5("<abbr title=\"", opt ? opt : txt, "\">", txt, "</abbr>");

	if (!strcmp(cmd, "section")) {
		assert(!opt);
		return concat4("</p><h4>", txt, "</h4><p>", "");
	}

	if (!strcmp(cmd, "subsection")) {
		assert(!opt);
		return concat4("</p><h5>", txt, "</h5><p>", "");
	}

	if (!strcmp(cmd, "subsubsection")) {
		assert(!opt);
		return concat4("</p><h6>", txt, "</h6><p>", "");
	}

	if (!strcmp(cmd, "wiki")) {
		return concat5("<a href=\"" WIKI_BASE_URL "/", txt,
			"\"><img src=\"/wiki.png\" alt=\"Wikipedia article:\" />&nbsp;",
			opt ? opt : txt, "</a>");
	}

	if (!strcmp(cmd, "bug")) {
		assert(!opt);
		return concat5("<a href=\"" BUG_BASE_URL "/", txt,
			"\"><img src=\"/bug.png\" alt=\"bug #\" />&nbsp;",
			txt, "</a>");
	}

	if (!strcmp(cmd, "degree")) {
		assert(!opt);
		return concat4("\xc2\xb0", txt, "", "");
	}

	return concat4("[INVAL CMD", txt, "]", "");
}

static char *dash(int len)
{
	static const char *ret[4] = {
		[0] = NULL,
		[1] = "-",
		[2] = "&ndash;",
		[3] = "&mdash;",
	};

	assert(len <= 3);

	return strdup(ret[len]);
}

static char *oquote(int len)
{
	static const char *ret[3] = {
		[0] = NULL,
		[1] = "&lsquo;",
		[2] = "&ldquo;",
	};

	assert(len <= 2);

	return strdup(ret[len]);
}

static char *cquote(int len)
{
	static const char *ret[3] = {
		[0] = NULL,
		[1] = "&rsquo;",
		[2] = "&rdquo;",
	};

	assert(len <= 2);

	return strdup(ret[len]);
}

static char *special_char(char *txt)
{
	char *ret;

	switch(*txt) {
		case '"': ret = "&quot;"; break;
		case '<': ret = "&lt;"; break;
		case '>': ret = "&gt;"; break;
		default:
			ret = txt;
			break;
	}

	return strdup(ret);
}

%}

%union {
	char *ptr;
};

%token <ptr> PAREND NLINE WSPACE BSLASH OCURLY CCURLY OBRACE CBRACE AMP
%token <ptr> USCORE PERCENT DOLLAR TILDE DASH OQUOT CQUOT SCHAR ELLIPSIS
%token <ptr> UTF8FIRST3 UTF8FIRST2 UTF8REST WORD PIPE

%type <ptr> paragraphs paragraph line thing cmd cmdarg optcmdarg

%%

post : paragraphs			{ data->output = $1; }
     | PAREND				{ data->output = strdup(""); }
     | NLINE				{ data->output = strdup(""); }
     |
     ;

paragraphs : paragraphs PAREND paragraph NLINE	{ $$ = concat4($1, "<p>", $3, "</p>\n"); }
           | paragraphs PAREND paragraph	{ $$ = concat4($1, "<p>", $3, "</p>\n"); }
	   | paragraph NLINE			{ $$ = concat4("<p>", $1, "</p>\n", ""); }
	   | paragraph				{ $$ = concat4("<p>", $1, "</p>\n", ""); }
	   ;

paragraph : paragraph NLINE line	{ $$ = concat4($1, " ", $3, ""); }
	  | line			{ $$ = $1; }
	  ;

line : line thing		{ $$ = concat($1, $2); }
     | thing			{ $$ = $1; }
     ;

thing : WORD				{ $$ = $1; }
      | UTF8FIRST2 UTF8REST		{ $$ = concat($1, $2); }
      | UTF8FIRST3 UTF8REST UTF8REST	{ $$ = concat4($1, $2, $3, ""); }
      | WSPACE				{ $$ = $1; }
      | PIPE				{ $$ = $1; }
      | DASH				{ $$ = dash(strlen($1)); }
      | OQUOT				{ $$ = oquote(strlen($1)); }
      | CQUOT				{ $$ = cquote(strlen($1)); }
      | SCHAR				{ $$ = special_char($1); }
      | ELLIPSIS			{ $$ = strdup("&hellip;"); }
      | TILDE				{ $$ = strdup("&nbsp;"); }
      | BSLASH cmd			{ $$ = $2; }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd(data->post, $1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd(data->post, $1, $2, NULL); }
    | BSLASH			{ $$ = strdup("<br/>"); }
    | OCURLY			{ $$ = $1; }
    | CCURLY			{ $$ = $1; }
    | OBRACE			{ $$ = $1; }
    | CBRACE			{ $$ = $1; }
    | AMP			{ $$ = strdup("&amp;"); }
    | USCORE			{ $$ = $1; }
    | PERCENT			{ $$ = $1; }
    | DOLLAR			{ $$ = $1; }
    | TILDE			{ $$ = $1; }
    ;

optcmdarg : OBRACE paragraph CBRACE	{ $$ = $2; }
          ;

cmdarg : OCURLY paragraph CCURLY	{ $$ = $2; }
       ;

%%
