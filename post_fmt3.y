%pure-parser
%lex-param {void *scanner}
%parse-param {struct parser_output *data}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/sha.h>

#include "config.h"
#include "listing.h"
#include "error.h"
#include "utils.h"
#include "mangle.h"

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

static char *concat(char *a, char *b)
{
	char *ret;

	ret = malloc(strlen(a) + strlen(b) + 1);
	ASSERT(ret);

	strcpy(ret, a);
	strcat(ret, b);

	return ret;
}

static char *concat4(char *a, char *b, char *c, char *d)
{
	char *ret;

	ret = malloc(strlen(a) + strlen(b) + strlen(c) + strlen(d) + 1);
	ASSERT(ret);

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
	ASSERT(ret);

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

static char *verbatim(char *txt)
{
	char *escaped;

	escaped = mangle_htmlescape(txt);
	ASSERT(escaped);

	return concat4("</p><pre>", escaped, "</pre><p>", "");
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
		ASSERT(!opt);
		return concat4("<em>", txt, "</em>", "");
	}

	if (!strcmp(cmd, "texttt")) {
		ASSERT(!opt);
		return concat4("<tt>", txt, "</tt>", "");
	}

	if (!strcmp(cmd, "textbf")) {
		ASSERT(!opt);
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
			ASSERT(!opt);
			return xstrdup(begin ? "</p><ol>" : "</ol><p>");
		}

		if (!strcmp(txt, "itemize")) {
			ASSERT(!opt);
			return xstrdup(begin ? "</p><ul>" : "</ul><p>");
		}

		if (!strcmp(txt, "description")) {
			ASSERT(!opt);
			return xstrdup(begin ? "</p><dl>" : "</dl><p>");
		}

		if (!strcmp(txt, "quote")) {
			ASSERT(!opt);
			return xstrdup(begin ? "</p><blockquote><p>" :
					      "</p></blockquote><p>");
		}

		return concat4("[INVAL ENVIRON", txt, "]", "");
	}

	if (!strcmp(cmd, "abbrev"))
		return concat5("<abbr title=\"", opt ? opt : txt, "\">", txt, "</abbr>");

	if (!strcmp(cmd, "section")) {
		ASSERT(!opt);
		return concat4("</p><h4>", txt, "</h4><p>", "");
	}

	if (!strcmp(cmd, "subsection")) {
		ASSERT(!opt);
		return concat4("</p><h5>", txt, "</h5><p>", "");
	}

	if (!strcmp(cmd, "subsubsection")) {
		ASSERT(!opt);
		return concat4("</p><h6>", txt, "</h6><p>", "");
	}

	if (!strcmp(cmd, "wiki")) {
		return concat5("<a href=\"" WIKI_BASE_URL "/", txt,
			"\"><img src=\"" BASE_URL "/wiki.png\" alt=\"Wikipedia article:\" />&nbsp;",
			opt ? opt : txt, "</a>");
	}

	if (!strcmp(cmd, "bug")) {
		ASSERT(!opt);
		return concat5("<a href=\"" BUG_BASE_URL "/", txt,
			"\"><img src=\"" BASE_URL "/bug.png\" alt=\"bug #\" />&nbsp;",
			txt, "</a>");
	}

	if (!strcmp(cmd, "degree")) {
		ASSERT(!opt);
		return concat4("\xc2\xb0", txt, "", "");
	}

	if (!strcmp(cmd, "tag") ||
	    !strcmp(cmd, "title")) {
		ASSERT(!opt);
		return xstrdup("");
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

	ASSERT(len <= 3);

	return xstrdup(ret[len]);
}

static char *oquote(int len)
{
	static const char *ret[3] = {
		[0] = NULL,
		[1] = "&lsquo;",
		[2] = "&ldquo;",
	};

	ASSERT(len <= 2);

	return xstrdup(ret[len]);
}

static char *cquote(int len)
{
	static const char *ret[3] = {
		[0] = NULL,
		[1] = "&rsquo;",
		[2] = "&rdquo;",
	};

	ASSERT(len <= 2);

	return xstrdup(ret[len]);
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

	return xstrdup(ret);
}

static inline char asciify(unsigned char c)
{
	return (c < 10) ? (c + '0') : (c - 10 + 'A');
}

static void hexdump(char *a, unsigned char *b, int len)
{
	int i;

	for(i=0; i<len; i++) {
		a[i*2]   = asciify(b[i] >> 4);
		a[i*2+1] = asciify(b[i] & 15);
	}
	a[i*2] = '\0';
}

#define TEX_TMP_DIR "/tmp"
#define TEX_FILE_NAME TEX_TMP_DIR "/blahg_math"
static int __render_math(char *tex, char *md, char *dstpath, char *texpath,
			 char *dvipath, char *pspath, char *pngpath)
{
	char cmd[3*FILENAME_MAX];
	char *pwd;
	FILE *f;

	pwd = getcwd(NULL, FILENAME_MAX);
	if (!pwd)
		goto err;

	f = fopen(texpath, "w");
	if (!f)
		goto err;

	chdir(TEX_TMP_DIR);

	fprintf(f, "\\documentclass[10pt]{article}\n");
	fprintf(f, "%% add additional packages here\n");
	fprintf(f, "\\usepackage{amsmath}\n");
	fprintf(f, "\\usepackage{amsfonts}\n");
	fprintf(f, "\\usepackage{amssymb}\n");
	//fprintf(f, "\\usepackage{pst-plot}\n");
	fprintf(f, "\\usepackage{color}\n");
	fprintf(f, "\\pagestyle{empty}\n");
	fprintf(f, "\\begin{document}\n");
	fprintf(f, "\\begin{equation*}\n");
	fprintf(f, "\\large\n");
	fprintf(f, tex);
	fprintf(f, "\\end{equation*}\n");
	fprintf(f, "\\end{document}\n");
	fclose(f);

	snprintf(cmd, sizeof(cmd), "latex --interaction=nonstopmode %s > /dev/null", texpath);
	if (system(cmd))
		goto err_chdir;

	snprintf(cmd, sizeof(cmd), "dvips -E %s -o %s > /dev/null", dvipath, pspath);
	if (system(cmd))
		goto err_chdir;

	snprintf(cmd, sizeof(cmd), "convert -density 120 %s %s > /dev/null", pspath, pngpath);
	if (system(cmd))
		goto err_chdir;

	chdir(pwd);

	snprintf(cmd, sizeof(cmd), "cp %s %s", pngpath, dstpath);
	if (system(cmd))
		goto err;

	return 0;

err_chdir:
	chdir(pwd);

err:
	return 1;
}

static char *render_math(char *tex)
{
	char finalpath[FILENAME_MAX];
	char texpath[FILENAME_MAX];
	char dvipath[FILENAME_MAX];
	char pspath[FILENAME_MAX];
	char pngpath[FILENAME_MAX];

	unsigned char md[20];
	char amd[41];
	int ret;

	SHA1((const unsigned char*) tex, strlen(tex), md);
	hexdump(amd, md, 20);

	snprintf(finalpath, FILENAME_MAX, "math/%s.png", amd);
	snprintf(texpath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.tex", amd, getpid());
	snprintf(dvipath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.dvi", amd, getpid());
	snprintf(pspath,  FILENAME_MAX, "/tmp/blahg_math_%s_%d.ps",  amd, getpid());
	snprintf(pngpath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.png", amd, getpid());

	ret = 0;
	//if (path does not exist)
		ret = __render_math(tex, amd, finalpath, texpath, dvipath,
				    pspath, pngpath);

	if (ret)
		return concat4("<span>Math Error (", strerror(errno),
			       ")</span>", "");

	return concat4("<img src=\"", finalpath, "\" alt=\"math\" />", "");
}

%}

%union {
	char *ptr;
};

%token <ptr> PAREND NLINE WSPACE BSLASH OCURLY CCURLY OBRACE CBRACE AMP
%token <ptr> USCORE PERCENT DOLLAR TILDE DASH OQUOT CQUOT SCHAR ELLIPSIS
%token <ptr> UTF8FIRST3 UTF8FIRST2 UTF8REST WORD PLUS MINUS ASTERISK SLASH
%token <ptr> PIPE
%token <ptr> W_TIMES W_FRAC
%token <ptr> VERBTEXT
%token VERBSTART VERBEND

%type <ptr> paragraphs paragraph line thing cmd cmdarg optcmdarg math mexpr
%type <ptr> mcmd mcmdarg
%type <ptr> verb

%left USCORE
%left PLUS MINUS
%left ASTERISK SLASH

%%

post : paragraphs			{ data->output = $1; }
     | PAREND				{ data->output = xstrdup(""); }
     | NLINE				{ data->output = xstrdup(""); }
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
      | PLUS				{ $$ = $1; }
      | MINUS				{ $$ = dash(strlen($1)); }
      | ASTERISK			{ $$ = $1; }
      | SLASH				{ $$ = $1; }
      | DASH				{ $$ = dash(strlen($1)); }
      | OQUOT				{ $$ = oquote(strlen($1)); }
      | CQUOT				{ $$ = cquote(strlen($1)); }
      | SCHAR				{ $$ = special_char($1); }
      | ELLIPSIS			{ $$ = xstrdup("&hellip;"); }
      | TILDE				{ $$ = xstrdup("&nbsp;"); }
      | BSLASH cmd			{ $$ = $2; }
      | DOLLAR math DOLLAR		{ $$ = render_math($2); }
      | VERBSTART verb VERBEND		{ $$ = verbatim($2); }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd(data->post, $1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd(data->post, $1, $2, NULL); }
    | BSLASH			{ $$ = xstrdup("<br/>"); }
    | OCURLY			{ $$ = $1; }
    | CCURLY			{ $$ = $1; }
    | OBRACE			{ $$ = $1; }
    | CBRACE			{ $$ = $1; }
    | AMP			{ $$ = xstrdup("&amp;"); }
    | USCORE			{ $$ = $1; }
    | PERCENT			{ $$ = $1; }
    | DOLLAR			{ $$ = $1; }
    | TILDE			{ $$ = $1; }
    ;

optcmdarg : OBRACE paragraph CBRACE	{ $$ = $2; }
          ;

cmdarg : OCURLY paragraph CCURLY	{ $$ = $2; }
       ;

verb : verb VERBTEXT			{ $$ = concat($1, $2); }
     | VERBTEXT				{ $$ = $1; }

math : mexpr			{ $$ = $1; }
     ;

mexpr : WORD				{ $$ = $1; }
      | WSPACE				{ $$ = $1; }
      | mexpr USCORE mexpr 		{ $$ = concat4($1, $2, $3, ""); }
      | mexpr PLUS mexpr 		{ $$ = concat4($1, $2, $3, ""); }
      | mexpr MINUS mexpr 		{ $$ = concat4($1, $2, $3, ""); }
      | mexpr ASTERISK mexpr	 	{ $$ = concat4($1, $2, $3, ""); }
//      | mexpr WSPACE mexpr 		{ $$ = concat4($1, $2, $3, ""); }
//      | OCURLY mexpr CCURLY		{ $$ = concat4($1, $2, $3, ""); }
//      | BSLASH WORD			{ $$ = concat($1, $2); }
//      | BSLASH mcmd			{ $$ = concat($1, $2); }
      |					{ $$ = strdup(""); }
      ;

mcmd : W_TIMES				{ $$ = $1; }
     | W_FRAC mcmdarg mcmdarg		{ $$ = concat4($1, $2, $3, ""); }
     ;

mcmdarg : OCURLY mexpr CCURLY		{ $$ = concat4($1, $2, $3, ""); }
	;

%%
