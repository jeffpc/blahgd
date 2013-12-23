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
 *
 * The entire conversion happens in several passes:
 *
 *  1) Convert parse tree to proto-AST.  This is essentially a node mapping
 *     phase - each ptnode gets mapped to a astnode.  There are two
 *     important properties of this pass:
 *
 *      a) commands capture arguments - any optional or mandatory arguments
 *         that aren't used will show up in the output
 *
 *      b) environments aren't yet processed - you'll see the \begin and
 *         \end commands in the output
 */

#if 0
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
		case PT_NL:
			an = astnode_new(AST_NL);
			break;
		case PT_NBSP:
			an = astnode_new(AST_NBSP);
			break;
		case PT_MATH:
			an = astnode_new_math(pn->u.str);
			break;
		default:
			ASSERT(0);
			break;
	}

	ASSERT(an);

	return an;
}

static struct astnode *__cvt(struct list_head *nodes)
{
	struct ptnode *pnode, *tmp;
	struct list_head paragraphs;
	struct list_head concat;
	struct astnode *last_cmd;
	struct astnode *anode;
	size_t cmd_nmand;	/* number of mandatory args we've seen */

	INIT_LIST_HEAD(&paragraphs);
	INIT_LIST_HEAD(&concat);
	last_cmd = NULL;
	cmd_nmand = 0;

	list_for_each_entry_safe(pnode, tmp, nodes, list) {
		switch (pnode->type) {
			case PT_STR:
			case PT_CHAR:
			case PT_NL:
			case PT_NBSP:
			case PT_MATH:
				/*
				 * these parse tree nodes convert trivially,
				 * and we just concat them
				 */
				anode = __cvt_ptnode(pnode);
				list_add_tail(&anode->list, &concat);
				break;
			case PT_ENV: {
				if (pnode->u.b) {
					/* we got a \begin */
					ASSERT(0);
				} else {
					/* we got a \end */
					ASSERT(0);
				}
				break;
			}
			case PT_CMD: {
				const struct ast_cmd *cmd;

				cmd = ast_cmd_lookup(pnode->u.str);
				ASSERT(cmd);

				anode = astnode_new_cmd(cmd);
				list_add_tail(&anode->list, &concat);

				last_cmd = anode;
				cmd_nmand = 0;
				break;
			}
			case PT_OPT_MAN: {
				struct astnode *child;

				/*
				 * Regardless of how we treat this parse
				 * tree node, we need to convert it into AST
				 * concat node.
				 */
				child = __cvt(&pnode->u.tree->nodes);
				ASSERT(child);

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
					list_add_tail(&child->list,
						      &last_cmd->u.cmd.mand);
				}
				break;
			}
			case PT_PAR: {
				struct astnode *par;

				/*
				 * We found a paragraph parse tree node,
				 * time to take everything we've seen and
				 * added to the concat queue, and turn it
				 * into a paragraph.
				 */

				par = astnode_new_par();

				list_splice(&concat, &par->u.concat);

				list_add_tail(&par->list, &paragraphs);
				break;
			}
			default:
				ASSERT(0);
				break;
		}

		if (last_cmd && (cmd_nmand == last_cmd->u.cmd.info->nmand))
			last_cmd = NULL;
	}

	/*
	 * If we have seen a paragraph, let's turn everything on the concat
	 * queue into another paragraph.
	 */
	if (!list_empty(&paragraphs)) {
		anode = astnode_new_par();

		list_splice(&concat, &anode->u.concat);

		list_add_tail(&anode->list, &paragraphs);
	}

	/*
	 * Now, we have three possible cases:
	 *  (1) no paragraphs, no concat nodes  -> return NULL
	 *  (2) paragraphs, no concat nodes     -> return paragraphs
	 *  (3) no paragraphs, concat nodes     -> return concat
	 */

	if (list_empty(&concat) && list_empty(&paragraphs))
		return NULL;

	anode = astnode_new_concat();

	if (!list_empty(&paragraphs))
		list_splice(&paragraphs, &anode->u.concat);
	else
		list_splice(&concat, &anode->u.concat);

	return anode;
}
#endif

static struct astnode *__pass1_cvt(struct list_head *nodes)
{
	struct ptnode *pnode, *pnode_tmp;
	struct astnode *envs[64];
	size_t idx;
	struct astnode *an;
	struct list_head items;

	INIT_LIST_HEAD(&items);

	idx = 0;
	envs[idx] = astnode_new_env(NULL);

	list_for_each_entry_safe(pnode, pnode_tmp, nodes, list) {
		switch (pnode->type) {
			case PT_STR:
			case PT_CHAR:
			case PT_NL:
			case PT_NBSP:
			case PT_MATH:
			case PT_VERB:
			case PT_CMD:
			case PT_PAR:
			case PT_OPT_MAN:
			case PT_OPT_OPT:
				list_del(&pnode->list);
				list_add_tail(&pnode->list, &items);
				break;
			case PT_ENV:
				if (pnode->u.b) {
					if (!list_empty(&items)) {
						/*
						 * 1) move everything from
						 * items into an ENCAP AST
						 * node
						 */
						an = astnode_new_encap(&items);

						/*
						 * 2) add the AST node to
						 * the topmost env
						 */
						list_add_tail(&an->list,
							      &envs[idx]->u.env.children);
					}

					/*
					 * define a new top-most env based
					 * on the PT_ENV we are looking at
					 * and add it to the list of
					 * children of the currently
					 * top-most env
					 */
					idx++;

					/* FIXME: pass in the env name */
					envs[idx] = astnode_new_env(NULL);

					list_add_tail(&envs[idx]->list,
						      &envs[idx - 1]->u.env.children);
				} else {
					if (!list_empty(&items)) {
						/*
						 * 1) move everything from
						 * items into an ENCAP AST
						 * node
						 */
						an = astnode_new_encap(&items);

						/*
						 * 2) add the AST node to
						 * the topmost env
						 */
						list_add_tail(&an->list,
							      &envs[idx]->u.env.children);
					}

					/*
					 * FIXME: make sure the env name matches
					 */
#if 0
					VERIFY0(strcmp(envs[idx]->u.env.name,
						       CURRENTNAME));
#endif

					ASSERT3U(idx, >=, 1);

					/*
					 * pop the topmost env...that's all
					 */
					idx--;
				}
				break;
		}
	}

	if (!list_empty(&items)) {
		/*
		 * 1) move everything from items into an ENCAP AST node
		 */
		an = astnode_new_encap(&items);

		/*
		 * 2) add the AST node to the topmost env
		 */
		list_add_tail(&an->list, &envs[idx]->u.env.children);
	}

	return envs[0];
}

static void pass1(struct ast *ast, struct list_head *nodes)
{
	struct astnode *node;

	node = __pass1_cvt(nodes);

	list_add_tail(&node->list, &ast->nodes);
}

struct ast *ptree2ast(struct ptree *pt)
{
	struct ast *ast;

	ast = ast_new();
	if (!ast)
		return NULL;

	pass1(ast, &pt->nodes);

	return ast;
}
