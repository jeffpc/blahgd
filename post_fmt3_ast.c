#include <stdlib.h>

#include "post_fmt3_ast.h"
#include "error.h"

static struct astnode *__cvt_ptnode(struct ptnode *pn)
{
	struct astnode *an;

	switch (pn->type) {
		case PT_STR:
			an = astnode_new_str(pn->u.str);
			break;
		case PT_CHAR:
			an = astnode_new_char(pn->u.ch.ch, pn->u.ch.len);
			break;
		case PT_MATH:
			an = astnode_new_math(pn->u.str);
			break;
		default:
			an = NULL;
			ASSERT(0);
			break;
	}

	ASSERT(an);

	return an;
}

static int __cvt(struct ast *ast, struct list_head *nodes)
{
	struct ptnode *pnode, *tmp;
	struct astnode *anode;
	struct list_head concat;

	INIT_LIST_HEAD(&concat);

	list_for_each_entry_safe(pnode, tmp, nodes, list) {
		switch (pnode->type) {
			case PT_STR:
			case PT_CHAR:
			case PT_MATH:
				/*
				 * these parse tree nodes convert trivially,
				 * and we just concat them
				 */
				anode = __cvt_ptnode(pnode);
				list_add_tail(&anode->list, &concat);
				break;
			default:
				ASSERT(0);
				break;
		}
	}

	if (!list_empty(&concat)) {
		anode = astnode_new_concat();

		list_splice(&concat, &anode->u.concat);

		list_add_tail(&anode->list, &ast->nodes);
	}

	return 0;
}

struct ast *ptree2ast(struct ptree *pt)
{
	struct ast *ast;

	ast = ast_new();
	if (!ast)
		return NULL;

	__cvt(ast, &pt->nodes);

	return ast;
}
