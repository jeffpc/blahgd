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

#include <openssl/sha.h>

#include "config.h"
#include "error.h"
#include "utils.h"
#include "mangle.h"
#include "listing.h"
#include "post_fmt3_cmds.h"

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
#define TEX_FILE_NAME TEX_TMP_DIR "/tmp"
static int __render_math(char *tex, char *md, char *dstpath, char *texpath,
			 char *dvipath, char *pngpath)
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

	fprintf(f, "\\documentclass{article}\n");
	fprintf(f, "%% add additional packages here\n");
	fprintf(f, "\\usepackage{amsmath}\n");
	fprintf(f, "\\usepackage{bm}\n");
	fprintf(f, "\\pagestyle{empty}\n");
	fprintf(f, "\\begin{document}\n");
	fprintf(f, "\\begin{equation*}\n");
	fprintf(f, "\\large\n");
	fprintf(f, tex);
	fprintf(f, "\\end{equation*}\n");
	fprintf(f, "\\end{document}\n");
	fclose(f);

	snprintf(cmd, sizeof(cmd), LATEX_BIN " --interaction=nonstopmode %s > /dev/null", texpath);
	LOG("math cmd: '%s'", cmd);
	if (system(cmd))
		goto err_chdir;

	snprintf(cmd, sizeof(cmd), DVIPNG_BIN " -T tight -x 1200 -z 9 "
			"-bg Transparent -o %s %s > /dev/null", pngpath, dvipath);
	LOG("math cmd: '%s'", cmd);
	if (system(cmd))
		goto err_chdir;

	chdir(pwd);

	snprintf(cmd, sizeof(cmd), "cp %s %s", pngpath, dstpath);
	LOG("math cmd: '%s'", cmd);
	if (system(cmd))
		goto err_chdir;

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
	char logpath[FILENAME_MAX];
	char auxpath[FILENAME_MAX];
	char dvipath[FILENAME_MAX];
	char pngpath[FILENAME_MAX];

	struct stat statbuf;
	unsigned char md[20];
	char amd[41];
	int ret;

	SHA1((const unsigned char*) tex, strlen(tex), md);
	hexdump(amd, md, 20);

	snprintf(finalpath, FILENAME_MAX, "math/%s.png", amd);
	snprintf(texpath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.tex", amd, getpid());
	snprintf(logpath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.log", amd, getpid());
	snprintf(auxpath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.aux", amd, getpid());
	snprintf(dvipath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.dvi", amd, getpid());
	snprintf(pngpath, FILENAME_MAX, "/tmp/blahg_math_%s_%d.png", amd, getpid());

	unlink(texpath);
	unlink(logpath);
	unlink(auxpath);
	unlink(dvipath);
	unlink(pngpath);

	ret = stat(finalpath, &statbuf);
	if (!ret)
		goto out;

	ret = __render_math(tex, amd, finalpath, texpath, dvipath, pngpath);

	unlink(texpath);
	unlink(logpath);
	unlink(auxpath);
	unlink(dvipath);
	unlink(pngpath);

	if (ret)
		return concat3(S("<span>Math Error ("), S(strerror(errno)),
			       S(")</span>"));

out:
	return concat5(S("<img src=\""), S(finalpath), S("\" alt=\"$"), tex, S("$\" />"));
}

%}

%union {
	char *ptr;
};

/* generic tokens */
%token <ptr> WSPACE BSLASH OCURLY CCURLY OBRACE CBRACE AMP
%token <ptr> USCORE PERCENT TILDE DASH OQUOT CQUOT SCHAR ELLIPSIS
%token <ptr> UTF8FIRST3 UTF8FIRST2 UTF8REST WORD ASTERISK SLASH
%token <ptr> PIPE
%token PAREND NLINE

/* math specific tokens */
%token <ptr> PLUS MINUS OPAREN CPAREN EQLTGT CARRET
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

post : paragraphs PAREND		{ data->output = $1; }
     | paragraphs			{ data->output = $1; }
     | PAREND				{ data->output = xstrdup(""); }
     ;

paragraphs : paragraphs PAREND paragraph	{ $$ = concat4($1, S("<p>"), $3, S("</p>\n")); }
	   | paragraph				{ $$ = concat3(S("<p>"), $1, S("</p>\n")); }
	   ;

paragraph : paragraph thing		{ $$ = concat($1, $2); }
          | thing			{ $$ = $1; }
          ;

thing : WORD				{ $$ = $1; }
      | UTF8FIRST2 UTF8REST		{ $$ = concat($1, $2); }
      | UTF8FIRST3 UTF8REST UTF8REST	{ $$ = concat3($1, $2, $3); }
      | NLINE				{ $$ = xstrdup(data->post->texttt_nesting ? "\n" : " "); }
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
      | AMP				{ $$ = xstrdup("</td><td>"); }
      | DOLLAR				{ $$ = xstrdup("$"); }
      | PERCENT				{ $$ = $1; }
      | BSLASH cmd			{ $$ = $2; }
      | MATHSTART math MATHEND		{ $$ = render_math($2); }
      | VERBSTART verb VERBEND		{ $$ = concat3(S("</p><p>"), $2, S("</p><p>")); }
      | LISTSTART verb LISTEND		{ $$ = concat3(S("</p><pre>"),
							 listing_str($2),
							 S("</pre><p>")); }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd(data->post, $1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd(data->post, $1, $2, NULL); }
    | WORD			{ $$ = process_cmd(data->post, $1, NULL, NULL); }
    | BSLASH			{ $$ = xstrdup("<br/>"); }
    | OCURLY			{ $$ = $1; }
    | CCURLY			{ $$ = $1; }
    | OBRACE			{ $$ = $1; }
    | CBRACE			{ $$ = $1; }
    | AMP			{ $$ = xstrdup("&amp;"); }
    | USCORE			{ $$ = $1; }
    | TILDE			{ $$ = $1; }
    ;

optcmdarg : OBRACE paragraph CBRACE	{ $$ = $2; }
          ;

cmdarg : OCURLY paragraph CCURLY	{ $$ = $2; }
       ;

verb : verb VERBTEXT			{ $$ = concat($1, $2); }
     | VERBTEXT				{ $$ = $1; }

math : math mexpr			{ $$ = concat($1, $2); }
     | mexpr				{ $$ = $1; }
     ;

mexpr : WORD				{ $$ = $1; }
      | WSPACE				{ $$ = $1; }
      | SCHAR				{ $$ = $1; }
      | mexpr EQLTGT mexpr 		{ $$ = concat3($1, $2, $3); }
      | mexpr USCORE mexpr 		{ $$ = concat3($1, $2, $3); }
      | mexpr CARRET mexpr 		{ $$ = concat3($1, $2, $3); }
      | mexpr PLUS mexpr 		{ $$ = concat3($1, $2, $3); }
      | mexpr MINUS mexpr 		{ $$ = concat3($1, $2, $3); }
      | mexpr ASTERISK mexpr	 	{ $$ = concat3($1, $2, $3); }
      | mexpr SLASH mexpr	 	{ $$ = concat3($1, $2, $3); }
      | BSLASH WORD			{ $$ = concat($1, $2); }
      | OPAREN math CPAREN		{ $$ = concat3($1, $2, $3); }
      | OCURLY math CCURLY		{ $$ = concat3($1, $2, $3); }
      ;

%%
