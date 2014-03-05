#ifndef __PARSE_H
#define __PARSE_H

#include "main.h"
#include "post.h"
#include "str.h"

struct parser_output {
	struct req *req;
	struct post *post;

	void *scanner;
	char *output;
	struct str *stroutput;

	char *input;
	size_t len;
	size_t pos;
	int lineno;
};

typedef void* yyscan_t;

extern int fmt3_lex_destroy(yyscan_t yyscanner);
extern int tmpl_lex_destroy(yyscan_t yyscanner);
extern int fmt3_parse(struct parser_output *data);
extern int tmpl_parse(struct parser_output *data);
extern void fmt3_set_extra(void * user_defined, yyscan_t yyscanner);
extern void tmpl_set_extra(void * user_defined, yyscan_t yyscanner);
extern int fmt3_lex_init(yyscan_t* scanner);
extern int tmpl_lex_init(yyscan_t* scanner);

#endif
