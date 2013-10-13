#ifndef __POST_FMT3_AST_H
#define __POST_FMT3_AST_H

#include "ptree.h"

enum asttype {
	AST_X = 0,
};

struct ast {
};

struct astnode {
	enum asttype type;
};

extern struct ast *ptree2ast(struct ptree *pt);

#endif
