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
				fprintf(stderr, "%*s    %p<->%p\n", indent, "",
					cur->u.encap.data.prev,
					cur->u.encap.data.next);
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
			case AST_ARG:
				fprintf(stderr, "%*s    %s arg\n",
					indent, "", cur->u.arg.opt ?
					"optional" : "mandatory");
				__ast_dump(&cur->u.arg.children, indent + 4);
				break;
			case AST_PAR:
				__ast_dump(&cur->u.par.children, indent + 2);
				break;
			case AST_ENV:
				fprintf(stderr, "%*s    >>%s<<\n", indent,
					"", cur->u.env.name);
				__ast_dump(&cur->u.env.children, indent + 2);
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

struct astnode *astnode_new_encap(struct list_head *data)
{
	struct astnode *node;

	node = astnode_new(AST_ENCAP);

	INIT_LIST_HEAD(&node->u.encap.data);

	if (!list_empty(data))
		list_splice(data, &node->u.encap.data);

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

struct astnode *astnode_new_env(char *name)
{
	struct astnode *node;

	node = astnode_new(AST_ENV);

	node->u.env.name = name;

	INIT_LIST_HEAD(&node->u.env.children);

	return node;
}

struct astnode *astnode_new_par()
{
	struct astnode *node;

	node = astnode_new(AST_PAR);

	INIT_LIST_HEAD(&node->u.par.children);

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

struct astnode *astnode_new_arg(bool opt)
{
	struct astnode *node;

	node = astnode_new(AST_ARG);

	node->u.arg.opt = opt;

	INIT_LIST_HEAD(&node->u.arg.children);

	return node;
}

static int __visit(struct list_head *list, struct ast *ast,
		   int (*fn)(struct ast *, struct astnode *))
{
	struct astnode *cur, *tmp;
	int ret;

	ret = 0;

	list_for_each_entry_safe(cur, tmp, list, list) {
		ret = fn(ast, cur);
		if (ret)
			break;

		switch (cur->type) {
			case AST_CMD:
				ret = __visit(&cur->u.cmd.mand, ast, fn);
				if (!ret)
					ret = __visit(&cur->u.cmd.opt, ast, fn);
				break;
			case AST_ARG:
				ret = __visit(&cur->u.arg.children, ast, fn);
				break;
			case AST_ENV:
				ret = __visit(&cur->u.env.children, ast, fn);
				break;
			case AST_PAR:
				ret = __visit(&cur->u.par.children, ast, fn);
				break;
			case AST_STR:
			case AST_CHAR:
			case AST_NL:
			case AST_NBSP:
			case AST_MATH:
			case AST_ENCAP:
				/* these have no children */
				ret = 0;
				break;
		}

		if (ret)
			break;
	}

	return ret;
}

int ast_visit(struct ast *ast, int (*fn)(struct ast *, struct astnode *))
{
	if (!fn)
		return 0;

	return __visit(&ast->nodes, ast, fn);
}
