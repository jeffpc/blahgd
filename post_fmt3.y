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
#include "str.h"

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

#warning chdir does not work for blahgd

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
		goto err;

	free(pwd);

	return 0;

err_chdir:
	chdir(pwd);

err:
	free(pwd);

	return 1;
}

static struct str *render_math(struct str *val)
{
	char finalpath[FILENAME_MAX];
	char texpath[FILENAME_MAX];
	char logpath[FILENAME_MAX];
	char auxpath[FILENAME_MAX];
	char dvipath[FILENAME_MAX];
	char pngpath[FILENAME_MAX];

	char *tex = val->str;
	struct stat statbuf;
	unsigned char md[20];
	char amd[41];
	int ret;

	SHA1((const unsigned char*) tex, strlen(tex), md);
	hexdump(amd, md, 20);

	snprintf(finalpath, FILENAME_MAX, "math/%s.png", amd);
	snprintf(texpath, FILENAME_MAX, "%s/blahg_math_%s_%d.tex", TEX_TMP_DIR, amd, getpid());
	snprintf(logpath, FILENAME_MAX, "%s/blahg_math_%s_%d.log", TEX_TMP_DIR, amd, getpid());
	snprintf(auxpath, FILENAME_MAX, "%s/blahg_math_%s_%d.aux", TEX_TMP_DIR, amd, getpid());
	snprintf(dvipath, FILENAME_MAX, "%s/blahg_math_%s_%d.dvi", TEX_TMP_DIR, amd, getpid());
	snprintf(pngpath, FILENAME_MAX, "%s/blahg_math_%s_%d.png", TEX_TMP_DIR, amd, getpid());

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

	if (ret) {
		str_putref(val);

		return str_cat3(STR_DUP("<span>Math Error ("),
		                STR_DUP(strerror(errno)),
			        STR_DUP(")</span>"));
	}

out:
	return str_cat5(STR_DUP("<img src=\""), STR_DUP(finalpath),
	                STR_DUP("\" alt=\"$"), val, STR_DUP("$\" />"));
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
