#include <stdlib.h>

#include "error.h"
#include "ast.h"
#include "utils.h"

struct ast *ast_new()
{
	struct ast *ast;

	ast = malloc(sizeof(struct ast));
	if (!ast)
		return NULL;

	INIT_LIST_HEAD(&ast->nodes);

	return ast;
}

static const char *ast_typename(enum asttype type)
{
	static const char *names[] = {
		[AST_STR]	= "AST_STR",
		[AST_CHAR]	= "AST_CHAR",
		[AST_NL]	= "AST_NL",
		[AST_NBSP]	= "AST_NBSP",
		[AST_MATH]	= "AST_MATH",
		[AST_CMD]	= "AST_CMD",
		[AST_ENV]	= "AST_ENV",
		[AST_CAT]	= "AST_CAT",
		[AST_PAR]	= "AST_PAR",
		[AST_ENCAP]	= "AST_ENCAP",
	};

	if ((type < AST_STR) || (type > AST_PAR))
		return "???";

	return names[type];
}

static void __ast_dump(struct list_head *nodes, int indent)
{
	struct astnode *cur, *tmp;

	list_for_each_entry_safe(cur, tmp, nodes, list) {
		fprintf(stderr, "%*s  %p - %s\n", indent, "", cur,
			ast_typename(cur->type));

		switch (cur->type) {
			case AST_ENCAP:
				fprintf(stderr, "%*s    %p\n", indent, "",
					cur->u.data);
				break;
			case AST_STR:
				fprintf(stderr, "%*s    >>%s<<\n", indent,
					"", cur->u.str);
				break;
			case AST_CMD:
				fprintf(stderr, "%*s    >>%s<<\n", indent,
					"", cur->u.cmd.info->name);
				fprintf(stderr, "%*s    mandatory args\n",
					indent, "");
				__ast_dump(&cur->u.cmd.mand, indent + 4);
				fprintf(stderr, "%*s    optional args\n",
					indent, "");
				__ast_dump(&cur->u.cmd.opt, indent + 4);
				break;
			case AST_CAT:
			case AST_PAR:
				__ast_dump(&cur->u.concat, indent + 2);
				break;
			case AST_NL:
			case AST_NBSP:
				break;
			default:
				ASSERT(0);
				break;
		}
	}
}

void ast_dump(struct ast *tree)
{
	fprintf(stderr, "AST %p\n", tree);

	__ast_dump(&tree->nodes, 0);
}

void ast_destroy(struct ast *ast)
{
	free(ast);
}

struct astnode *astnode_new(enum asttype type)
{
	struct astnode *node;

	node = malloc(sizeof(struct astnode));
	ASSERT(node);

	node->type = type;

	return node;
}

struct astnode *astnode_new_encap(void *data)
{
	struct astnode *node;

	node = astnode_new(AST_ENCAP);
	node->u.data = data;

	return node;
}

struct astnode *astnode_new_str(char *str)
{
	struct astnode *node;

	node = astnode_new(AST_STR);
	node->u.str = xstrdup(str);

	return node;
}

struct astnode *astnode_new_char(char ch, int len)
{
	struct astnode *node;

	node = astnode_new(AST_CHAR);

	return node;
}

struct astnode *astnode_new_math(char *str)
{
	struct astnode *node;

	node = astnode_new(AST_MATH);

	return node;
}

struct astnode *astnode_new_concat()
{
	struct astnode *node;

	node = astnode_new(AST_CAT);

	INIT_LIST_HEAD(&node->u.concat);

	return node;
}

struct astnode *astnode_new_par()
{
	struct astnode *node;

	node = astnode_new(AST_PAR);

	INIT_LIST_HEAD(&node->u.concat);

	return node;
}

struct astnode *astnode_new_cmd(const struct ast_cmd *cmd)
{
	struct astnode *node;

	node = astnode_new(AST_CMD);

	node->u.cmd.info = cmd;

	INIT_LIST_HEAD(&node->u.cmd.mand);
	INIT_LIST_HEAD(&node->u.cmd.opt);

	return node;
}
