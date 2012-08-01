%option nounput
%option noinput
%option reentrant
%option bison-bridge

%{
#include <stdlib.h>

#include "template.tab.h"
%}

%%
"{"			{ } /* tmpl_lval.c = *yytext; return OCURLY; } */
"}"			{ } /* tmpl_lval.c = *yytext; return CCURLY; } */
"|"			{ } /* tmpl_lval.c = *yytext; return PIPE; } */
"%"			{ } /* tmpl_lval.c = *yytext; return FORMAT; } */
[A-Za-z0-9]		{ } /* tmpl_lval.c = *yytext; return LETTER; } */
.			{ } /* tmpl_lval.c = *yytext; return CHAR; } */
%%

int tmpl_wrap(yyscan_t scanner)
{
	struct yyguts_t * yyg = (struct yyguts_t*) scanner;

	fclose(yyin);
	return 1;
}
