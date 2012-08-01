%pure-parser
%lex-param {void *scanner}
%parse-param {void *scanner}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"

void yyerror(void *scan, char *e)
{
	fprintf(stderr, "Error: %s\n", e);
}
%}

%union {
	char c;
};

%token <c> OCURLY CCURLY PIPE FORMAT
%token <c> LETTER CHAR

%%

page : words
     ;

words : words CHAR
      | words LETTER
      | words PIPE
      | words FORMAT
      | words cmd
      |
      ;

cmd : OCURLY name pipeline CCURLY
    | OCURLY name FORMAT name CCURLY
    | OCURLY name CCURLY
    ;

pipeline : pipeline pipe
	 | pipe
         ;

pipe : PIPE name
     ;

name : name LETTER
     | LETTER
     ;

%%
