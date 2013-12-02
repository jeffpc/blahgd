#include <stdlib.h>

#include "post_fmt3_ast.h"
#include "error.h"

/*
 * Converting the parse tree is relatively easy, but it does take some
 * effort.
 *
 * First of all, the parse tree is a nested stream of tokens.  It is not a
 * simple linear stream that the lexer spits out, but rather certain sets of
 * tokens are nested within parse nodes.  For example, 'foo {bar} baz' will
 * turn into a PT_STR ("foo"), PT_STR (space), PT_OPT_MAN, PT_STR (space),
 * PT_STR ("baz").  The PT_OPT_MAN's interpretation will be context
 * sensitive.  The code in this file is responsible for the conversion.  In
 * this case, since PT_OPT_MAN is not following a command, it should be
 * treated as a string of the contents (i.e., the {} get silently dropped).
 *
 * There are several different bits of context that we need to keep track
 * of.  In no particular order, they are:
 *
 *  1) command invocation - we need to know the most recent command
 *     invocation that is affecting the current parse node.  For example, in
 *     "\mbox{foo}{bar}", the "{foo}" node is affected by the \mbox command
 *     but "{bar}" is not.  The parse node stream will have a sequence of
 *     PT_CMD, PT_OPT_MAN, and PT_OPT_MAN.
 *
 *     The command table needs to inform us about how many optional and
 *     mandatory arguments to expect.
 *
 *  2) environments - consider this example: "\begin{foo}xyzzy\end{foo}".
 *     The parse node stream will simply contain PT_CMD, PT_OPT_MAN, PT_STR,
 *     PT_CMD, and PT_OPT_MAN.  Obiously, the PT_CMDs affect the immediate
 *     PT_OPT_MANs (see point #1).  However, as part of the AST
 *     construction, we want to turn the \begin & \end pair into an AST_ENV
 *     with all the in-between stuff underneath it.
 *
 *  3) environment nesting - environments can be nested, but the \begin and
 *     \end must happen at the same nesting level and commands cannot start
 *     outside of an environment and end inside or vice versa.
 */

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
	const struct ast_cmd *last_cmd;
	struct ptnode *pnode, *tmp;
	struct list_head concat;
	struct astnode *anode;
	size_t cmd_nmand;	/* number of mandatory args we've seen */
	size_t cmd_nopt;	/* number of optional args we've seen */

	INIT_LIST_HEAD(&concat);
	last_cmd = NULL;
	cmd_nmand = 0;
	cmd_nopt = 0;

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
			case PT_CMD: {
				const struct ast_cmd *cmd;

				cmd = ast_cmd_lookup(pnode->u.str);
				ASSERT(cmd);

				anode = astnode_new_cmd(cmd);
				list_add_tail(&anode->list, &concat);

				last_cmd = cmd;
				cmd_nmand = 0;
				cmd_nopt = 0;
				break;
			}
			case PT_OPT_MAN: {
				struct astnode *child;

				/*
				 * Regardless of how we treat this parse
				 * tree node, we need to convert it into AST
				 * node.
				 */

				if (!last_cmd) {
					/*
					 * treat '{foo}' as if the {} didn't
					 * exist
					 */
					ASSERT(0);
				} else {
					/*
					 * this is part of the last
					 * command's arguments
					 */
					cmd_nmand++;
					ASSERT(0);
				}
				break;
			}
			default:
				ASSERT(0);
				break;
		}

		if (last_cmd && (cmd_nmand == last_cmd->nmand))
			last_cmd = NULL;
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
