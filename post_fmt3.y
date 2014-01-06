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
#include "vars.h"

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

static struct val *__char_cvt(struct val *val, const char **outstr, size_t nouts)
{
	size_t len;

	ASSERT3U(val->type, ==, VT_STR);

	len = strlen(val->str);

	ASSERT3U(len, <=, nouts);

	val_putref(val);

	return VAL_DUP_STR(outstr[len]);
}

#define dash(v)		__char_cvt((v), dashes, ARRAY_LEN(dashes))
#define oquote(v)	__char_cvt((v), oquotes, ARRAY_LEN(oquotes))
#define cquote(v)	__char_cvt((v), cquotes, ARRAY_LEN(cquotes))

static struct val *special_char(struct val *val)
{
	char *ret;

	ASSERT3U(val->type, ==, VT_STR);
	ASSERT3U(strlen(val->str), ==, 1);

	switch (val->str[0]) {
		case '"': ret = "&quot;"; break;
		case '<': ret = "&lt;"; break;
		case '>': ret = "&gt;"; break;
		default:
			return val;
	}

	val_putref(val);

	return VAL_DUP_STR(ret);
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

static struct val *render_math(struct val *val)
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

	ASSERT3U(val->type, ==, VT_STR);

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
		val_putref(val);

		return valcat3(VAL_DUP_STR("<span>Math Error ("),
		               VAL_DUP_STR(strerror(errno)),
			       VAL_DUP_STR(")</span>"));
	}

out:
	return valcat5(VAL_DUP_STR("<img src=\""), VAL_DUP_STR(finalpath),
	               VAL_DUP_STR("\" alt=\"$"), val, VAL_DUP_STR("$\" />"));
}

%}

%union {
	struct val *ptr;
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

post : paragraphs PAREND		{ data->valoutput = $1; }
     | paragraphs			{ data->valoutput = $1; }
     | PAREND				{ data->valoutput = VAL_DUP_STR(""); }
     ;

paragraphs : paragraphs PAREND paragraph	{ $$ = valcat4($1, VAL_DUP_STR("<p>"), $3, VAL_DUP_STR("</p>\n")); }
	   | paragraph				{ $$ = valcat3(VAL_DUP_STR("<p>"), $1, VAL_DUP_STR("</p>\n")); }
	   ;

paragraph : paragraph thing		{ $$ = valcat($1, $2); }
          | thing			{ $$ = $1; }
          ;

thing : WORD				{ $$ = $1; }
      | UTF8FIRST2 UTF8REST		{ $$ = valcat($1, $2); }
      | UTF8FIRST3 UTF8REST UTF8REST	{ $$ = valcat3($1, $2, $3); }
      | NLINE				{ $$ = VAL_DUP_STR(data->post->texttt_nesting ? "\n" : " "); }
      | WSPACE				{ $$ = $1; }
      | PIPE				{ $$ = VAL_DUP_STR("|"); }
      | ASTERISK			{ $$ = VAL_DUP_STR("*"); }
      | SLASH				{ $$ = VAL_DUP_STR("/"); }
      | DASH				{ $$ = dash($1); }
      | OQUOT				{ $$ = oquote($1); }
      | CQUOT				{ $$ = cquote($1); }
      | SCHAR				{ $$ = special_char($1); }
      | ELLIPSIS			{ $$ = VAL_DUP_STR("&hellip;"); }
      | TILDE				{ $$ = VAL_DUP_STR("&nbsp;"); }
      | AMP				{ $$ = VAL_DUP_STR("</td><td>"); }
      | DOLLAR				{ $$ = VAL_DUP_STR("$"); }
      | PERCENT				{ $$ = VAL_DUP_STR("%"); }
      | BSLASH cmd			{ $$ = $2; }
      | MATHSTART math MATHEND		{ $$ = render_math($2); }
      | VERBSTART verb VERBEND		{ $$ = valcat3(VAL_DUP_STR("</p><p>"), $2, VAL_DUP_STR("</p><p>")); }
      | LISTSTART verb LISTEND		{ $$ = valcat3(VAL_DUP_STR("</p><pre>"),
							 listing_str($2),
							 VAL_DUP_STR("</pre><p>")); }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd(data->post, $1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd(data->post, $1, $2, NULL); }
    | WORD			{ $$ = process_cmd(data->post, $1, NULL, NULL); }
    | BSLASH			{ $$ = VAL_DUP_STR("<br/>"); }
    | OCURLY			{ $$ = VAL_DUP_STR("{"); }
    | CCURLY			{ $$ = VAL_DUP_STR("}"); }
    | OBRACE			{ $$ = VAL_DUP_STR("["); }
    | CBRACE			{ $$ = VAL_DUP_STR("]"); }
    | AMP			{ $$ = VAL_DUP_STR("&amp;"); }
    | USCORE			{ $$ = VAL_DUP_STR("_"); }
    | TILDE			{ $$ = VAL_DUP_STR("~"); }
    ;

optcmdarg : OBRACE paragraph CBRACE	{ $$ = $2; }
          ;

cmdarg : OCURLY paragraph CCURLY	{ $$ = $2; }
       ;

verb : verb VERBTEXT			{ $$ = valcat($1, $2); }
     | VERBTEXT				{ $$ = $1; }

math : math mexpr			{ $$ = valcat($1, $2); }
     | mexpr				{ $$ = $1; }
     ;

mexpr : WORD				{ $$ = $1; }
      | WSPACE				{ $$ = $1; }
      | SCHAR				{ $$ = $1; }
      | mexpr EQLTGT mexpr 		{ $$ = valcat3($1, $2, $3); }
      | mexpr USCORE mexpr 		{ $$ = valcat3($1, VAL_DUP_STR("_"), $3); }
      | mexpr CARRET mexpr 		{ $$ = valcat3($1, $2, $3); }
      | mexpr PLUS mexpr 		{ $$ = valcat3($1, $2, $3); }
      | mexpr MINUS mexpr 		{ $$ = valcat3($1, $2, $3); }
      | mexpr ASTERISK mexpr	 	{ $$ = valcat3($1, VAL_DUP_STR("*"), $3); }
      | mexpr SLASH mexpr	 	{ $$ = valcat3($1, VAL_DUP_STR("/"), $3); }
      | BSLASH WORD			{ $$ = valcat(VAL_DUP_STR("\\"), $2); }
      | OPAREN math CPAREN		{ $$ = valcat3(VAL_DUP_STR("("), $2, VAL_DUP_STR(")")); }
      | OCURLY math CCURLY		{ $$ = valcat3(VAL_DUP_STR("{"), $2, VAL_DUP_STR("}")); }
      ;

%%
