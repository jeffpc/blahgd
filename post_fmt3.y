%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int yylex(void);
void yyerror(char *);

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

static char *process_cmd(char *cmd, char *txt, char *opt)
{
	if (!strcmp(cmd, "link"))
		return concat5("<a href=\"", opt ? opt : "", "\">", txt, "</a>");

	if (!strcmp(cmd, "emph")) {
		assert(!opt);
		return concat4("<em>", txt, "</em>", "");
	}

	if (!strcmp(cmd, "item")) {
		assert(!opt);
		return concat4("<li>", txt, "</li>", "");
	}

	if (!strcmp(cmd, "section")) {
		assert(!opt);
		return concat4("<h4>", txt, "</h4>", "");
	}

	if (!strcmp(cmd, "subsection")) {
		assert(!opt);
		return concat4("<h5>", txt, "</h5>", "");
	}

	if (!strcmp(cmd, "subsubsection")) {
		assert(!opt);
		return concat4("<h6>", txt, "</h6>", "");
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
%token <ptr> USCORE DASH OQUOT CQUOT SCHAR WORD

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

thing : WORD			{ $$ = $1; }
      | WSPACE			{ $$ = $1; }
      | DASH			{ $$ = dash(strlen($1)); }
      | OQUOT			{ $$ = oquote(strlen($1)); }
      | CQUOT			{ $$ = cquote(strlen($1)); }
      | SCHAR			{ $$ = special_char($1); }
      | BSLASH cmd		{ $$ = $2; }
      ;

cmd : WORD optcmdarg cmdarg	{ $$ = process_cmd($1, $3, $2); }
    | WORD cmdarg		{ $$ = process_cmd($1, $2, NULL); }
    | BSLASH			{ $$ = strdup("<br/>"); }
    | OCURLY			{ $$ = $1; }
    | CCURLY			{ $$ = $1; }
    | OBRACE			{ $$ = $1; }
    | CBRACE			{ $$ = $1; }
    | AMP			{ $$ = $1; }
    | USCORE			{ $$ = $1; }
    ;

optcmdarg : OBRACE paragraph CBRACE	{ $$ = $2; }
          ;

cmdarg : OCURLY paragraph CCURLY	{ $$ = $2; }
       ;
