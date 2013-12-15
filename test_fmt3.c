#include <stdlib.h>

#include "post_fmt3_ast.h"
#include "parse.h"
#include "ast.h"
#include "error.h"
#include "utils.h"

static int onefile(struct post *post, char *ibuf, size_t len)
{
	struct parser_output x;
	struct ast *ast;
	int ret;

	x.req   = NULL;
	x.post  = post;
	x.input = ibuf;
	x.len   = strlen(ibuf);
	x.pos   = 0;

	post->table_nesting = 0;

	fmt3_lex_init(&x.scanner);
	fmt3_set_extra(&x, x.scanner);

	ret = fmt3_parse(&x);
	if (!ret) {
		ASSERT(x.ptree);
		pt_dump(x.ptree);

		ast = ptree2ast(x.ptree);
		if (ast) {
			ast_dump(ast);
			ast_destroy(ast);
		} else {
			fprintf(stderr, "failed to convert parse tree to AST\n");
			ret = 1;
		}

		pt_destroy(x.ptree);
	} else {
		fprintf(stderr, "failed to parse\n");
	}

	fmt3_lex_destroy(x.scanner);

	return ret;
}

int main(int argc, char **argv)
{
	struct post post;
	char *in;
	int i;
	int result;

	result = 0;

	ASSERT0(putenv("UMEM_DEBUG=default,verbose"));
	ASSERT0(putenv("BLAHG_DISABLE_SYSLOG=1"));

	for (i = 1; i < argc; i++) {
		in = read_file(argv[i]);
		ASSERT(in);

		if (onefile(&post, in, strlen(in)))
			result = 1;

		free(in);
	}

	return result;
}
