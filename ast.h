#ifndef __AST_H
#define __AST_H

#include <stdbool.h>

#include "list.h"

enum asttype {
	AST_STR = 1,	/* must be first */
	AST_CHAR,
	AST_NL,
	AST_NBSP,
	AST_MATH,
	AST_CMD,
	AST_ENV,
	AST_ARG,
	AST_ENCAP,
	AST_PAR,	/* must be last */
};

struct ast_cmd {
	const char *name;
	size_t nmand;	/* number of mandatory args */
	size_t nopt;	/* number of optional args */
};

struct ast {
	struct list_head nodes;
};

struct astnode {
	struct list_head list;
	enum asttype type;
	union {
		char *str;
		struct {
			char *name;
			struct list_head children;
		} env;
		struct {
			struct list_head children;
		} par;
		struct {
			const struct ast_cmd *info;
			struct list_head mand;
			struct list_head opt;
		} cmd;
		struct {
			struct list_head children;
			bool opt;
		} arg;
		struct {
			struct list_head data;
		} encap;
	} u;
};

extern struct ast *ast_new();
extern void ast_dump(struct ast *tree);
extern void ast_destroy(struct ast *tree);
extern int ast_visit(struct ast *ast, int (*fn)(struct ast *, struct astnode *));

extern struct astnode *astnode_new(enum asttype);
extern struct astnode *astnode_new_encap(struct list_head *);
extern struct astnode *astnode_new_str(char *);
extern struct astnode *astnode_new_char(char, int);
extern struct astnode *astnode_new_math(char *);
extern struct astnode *astnode_new_env(char *);
extern struct astnode *astnode_new_par();
extern struct astnode *astnode_new_cmd(const struct ast_cmd *);
extern struct astnode *astnode_new_arg(bool opt);

extern const struct ast_cmd *ast_cmd_lookup(const char *);

#endif
