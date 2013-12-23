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
 *     The parse node stream will simply contain PT_ENV, PT_STR, and PT_ENV.
 *     As part of the AST construction, we want to turn the \begin & \end
 *     pair into an AST_ENV with all the in-between stuff underneath it.
 *
 *  3) environment nesting - environments can be nested, but the \begin and
 *     \end must happen at the same nesting level and commands cannot start
 *     outside of an environment and end inside or vice versa.
 *
 * The entire conversion happens in several passes:
 *
 *  1) Scan through ptree for \begin and \end and capture the arguments.
 *     Look up the env (this could be hard because the name of the env is in
 *     an argument!) and turn the content PT nodes into an ENV AST node that
 *     has the encapsulation AST node as the content (the args need to be
 *     converted).
 *
 *     At the end of this pass, we have an AST where environments are
 *     correct and the parse tree is no longer useful; the resulting AST
 *     tree must not contain any PT_ENV nodes.
 *
 *  2) Scan through the AST looking for paragraph breaks in the encapsulated
 *     parse tree nodes.  Convert the content between breaks into PAR AST
 *     nodes with the content encapsulated.
 *
 *     At the end of this pass, the resulting AST must not contain any
 *     PT_ENV and PT_PAR nodes.
 *
 *  3) Scan through the AST looking for commands in the encapsulated parse
 *     tree nodes.  Look up the commands and capture the arguments - emit a
 *     CMD AST node for each command.  Any non-command parse tree nodes get
 *     put into AST encap node.
 *
 *     At the end of this pass, the resulting AST must not contain any
 *     PT_ENV, PT_PAR, or PT_CMD nodes.
 *
 *  4) Scan through the AST, converting any left PT_* nodes into AST nodes.
 *     STR, CHAR, NL, NBSP, MATH, and VERB are trivially converted into
 *     their AST equivalents.  Any OPT_MAN and OPT_OPT parse tree nodes
 *     still in the tree at this point haven't been captured by any commands
 *     or environments.  Therefore, they should be converted to string AST
 *     nodes.
 *
 *     XXX: tables
 *     XXX: \\ handling
 *
 *     At the end of this pass, the resulting AST must not contain any AST
 *     encapsulation nodes (and therefore any parse tree nodes).  We have a
 *     pure AST now!
 *
 * After these passes, the AST is renderable.  However, it may be
 * advantageous to perform some optimization passes.  Note, if the ptree2ast
 * conversion is too slow, it may be worthwhile to do a simple optimization
 * pass on the ptree - to reduce the number of nodes.
 */

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

static void __pass2_encap(struct ast *ast, struct astnode *node)
{
	struct ptnode *pnode, *pnode_tmp;

	ASSERT3U(node->type, ==, AST_ENCAP);

	list_for_each_entry_safe(pnode, pnode_tmp, &node->u.encap.data, list) {
		switch (pnode->type) {
			case PT_STR:
			case PT_CHAR:
			case PT_NL:
			case PT_NBSP:
			case PT_MATH:
			case PT_VERB:
			case PT_CMD:
			case PT_OPT_MAN:
			case PT_OPT_OPT:
				break;
			case PT_ENV:
				/* we shouldn't have these anymore */
				ASSERT(0);
				break;
			case PT_PAR:
				/* FIXME: move the contents of this list
				 * before this pnode into a separate ENCAP
				 * AST node, then move the new ENCAP AST
				 * node into a new PAR AST node.  Insert the
				 * new PAR AST node before node->list.
				 * Then, remove the PT_PAR and free it.
				 */
				ASSERT(0);
				break;
		}
	}

	if (list_empty(&node->u.encap.data)) {
		/* FIXME: remove & free this AST_ENCAP because it's empty */
	}
}

static int __pass2(struct ast *ast, struct astnode *node)
{
	fprintf(stderr, "%s: %p\n", __func__, node);

	switch (node->type) {
		case AST_STR:
		case AST_CHAR:
		case AST_NL:
		case AST_NBSP:
		case AST_MATH:
		case AST_CMD:
		case AST_PAR:
			/* we shouldn't have any of these yet */
			ASSERT(0);
			break;
		case AST_ENV:
			break;
		case AST_ENCAP:
			__pass2_encap(ast, node);
			break;
	}

	return 0;
}

static void pass2(struct ast *ast)
{
	ast_visit(ast, __pass2);
}

struct ast *ptree2ast(struct ptree *pt)
{
	struct ast *ast;

	ast = ast_new();
	if (!ast)
		return NULL;

	fprintf(stderr, "%s: pass 1\n", __func__);
	pass1(ast, &pt->nodes);
	ast_dump(ast);

	fprintf(stderr, "%s: pass 2\n", __func__);
	pass2(ast);
	ast_dump(ast);

	return ast;
}
