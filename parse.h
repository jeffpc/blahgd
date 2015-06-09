#ifndef __PARSE_H
#define __PARSE_H

#include "req.h"
#include "post.h"
#include "error.h"
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

	/* template related */
	bool cond_stack[COND_STACK_DEPTH];
	int cond_stack_use;
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

static inline bool cond_value(struct parser_output *data)
{
	return ((data->cond_stack_use == -1) ||
		data->cond_stack[data->cond_stack_use]);
}

static inline void cond_if(struct parser_output *data, bool val)
{
	ASSERT3S(data->cond_stack_use, <, COND_STACK_DEPTH);

	data->cond_stack[++data->cond_stack_use] = val;
}

static inline void cond_else(struct parser_output *data)
{
	ASSERT3S(data->cond_stack_use, >=, 0);

	data->cond_stack[data->cond_stack_use] = !data->cond_stack[data->cond_stack_use];
}

static inline void cond_endif(struct parser_output *data)
{
	ASSERT3S(data->cond_stack_use, >=, 0);

	data->cond_stack_use--;
}

#endif
