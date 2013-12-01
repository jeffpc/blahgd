#ifndef __AST_H
#define __AST_H

#include "list.h"

enum asttype {
	AST_STR = 1,	/* must be first */
	AST_CHAR,
	AST_MATH,
	AST_CMD,
	AST_ENV,
	AST_CAT,
	AST_PAR,	/* must be last */
};

struct ast {
	struct list_head nodes;
};

struct astnode {
	struct list_head list;
	enum asttype type;
	union {
		char *str;
		struct list_head concat;
	} u;
};

extern struct ast *ast_new();
extern void ast_dump(struct ast *tree);
extern void ast_destroy(struct ast *tree);

extern struct astnode *astnode_new(enum asttype);
extern struct astnode *astnode_new_str(char *);
extern struct astnode *astnode_new_char(char, int);
extern struct astnode *astnode_new_math(char *);
extern struct astnode *astnode_new_concat();

#endif
