%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "post.h"
#include "listing.h"

static struct post *post;

extern int yylex(void);
extern void yyrestart(FILE*);

void yyerror(char *e)
{
	fprintf(post->out, "Error: %s\n", e);
}

void yyerror2(char *e, char *yytext)
{
	fprintf(post->out, "Error: %s (%s)\n", e, yytext);
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

static char *__listing(char *txt, char *opt)
{
	return concat4("</p><pre>", "", listing(post, txt), "</pre><p>");
}

static char *process_cmd(char *cmd, char *txt, char *opt)
{
	if (!strcmp(cmd, "link"))
		return concat5("<a href=\"", opt ? opt : "", "\">", txt, "</a>");

	if (!strcmp(cmd, "img"))
		return concat5("<img src=\"", txt, "\" alt=\"", opt ? opt : "", "\" />");

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
		return __listing(txt, opt);

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

	if (!strcmp(cmd, "bug")) {
		assert(!opt);
		return concat5("<a href=\"" BUG_BASE_URL "/", txt,
			"\"><img src=\"/static/bug.png\" alt=\"bug #\" />&nbsp;",
			txt, "</a>");
	}

	return concat4("[INVAL CMD", txt, "]", "");
}

static char *dash(int len)
{
	char *ret[4] = {
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
	char *ret[3] = {
		[0] = NULL,
		[1] = "&lsquo;",
		[2] = "&ldquo;",
	};

	assert(len <= 2);

	return strdup(ret[len]);
}

static char *cquote(int len)
{
	char *ret[3] = {
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
%token <ptr> UTF8FIRST3 UTF8FIRST2 UTF8REST WORD

%type <ptr> paragraphs paragraph line thing cmd cmdarg optcmdarg

%%

post : paragraphs			{ printf("%s\n", $1); }
     | PAREND
     | NLINE
     |
     ;

paragraphs : paragraphs PAREND paragraph NLINE	{ $$ = concat4($1, "<p>", $3, "</p>\n"); }
           | paragraphs PAREND paragraph	{ $$ = concat4($1, "<p>", $3, "</p>\n"); }
	   | paragraph NLINE			{ $$ = concat4("<p>", $1, "</p>\n", ""); }
	   | paragraph				{ $$ = concat4("<p>", $1, "</p>\n", ""); }
	   ;

paragraph : paragraph NLINE line	{ $$ = concat4($1, $2, $3, ""); }
	  | line			{ $$ = $1; }
	  ;

line : line thing		{ $$ = concat($1, $2); }
     | thing			{ $$ = $1; }
     ;

thing : WORD				{ $$ = $1; }
      | UTF8FIRST2 UTF8REST		{ $$ = concat($1, $2); }
      | UTF8FIRST3 UTF8REST UTF8REST	{ $$ = concat4($1, $2, $3, ""); }
      | WSPACE				{ $$ = $1; }
      | DASH				{ $$ = dash(strlen($1)); }
      | OQUOT				{ $$ = oquote(strlen($1)); }
      | CQUOT				{ $$ = cquote(strlen($1)); }
      | SCHAR				{ $$ = special_char($1); }
      | ELLIPSIS			{ $$ = strdup("&hellip;"); }
      | TILDE				{ $$ = strdup("&nbsp;"); }
      | BSLASH cmd			{ $$ = $2; }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd($1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd($1, $2, NULL); }
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

void __do_cat_post_fmt3(struct post *p, char *path)
{
	FILE *f;

	f = fopen(path, "r");
	if (!f) {
		fprintf(p->out, "post.tex open error\n");
		return;
	}

	post = p;

	yyrestart(f);

	while (yyparse())
		;
}
