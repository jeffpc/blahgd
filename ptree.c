#include <stdlib.h>

#include "ptree.h"
#include "list.h"
#include "error.h"

struct ptnode *ptn_new(enum pttype type)
{
	struct ptnode *ptn;

	ptn = malloc(sizeof(struct ptnode));
	ASSERT(ptn);

	ptn->type = type;

	switch (type) {
		case PT_CHAR:
		case PT_STR:
		case PT_NL:
		case PT_NBSP:
		case PT_MATH:
		case PT_VERB:
		case PT_CMD:
		case PT_ENV:
		case PT_PAR:
		case PT_OPT_MAN:
		case PT_OPT_OPT:
		case PT_TBL_COL:
			/* nothing */
			break;
		default:
			ASSERT(0);
	}

	return ptn;
}

static struct ptnode *__ptn_new_str(enum pttype type, char *str)
{
	struct ptnode *ptn;

	ptn = ptn_new(type);
	ASSERT(ptn);

	ptn->u.str = str;

	return ptn;
}

struct ptnode *ptn_new_str(char *str)
{
	return __ptn_new_str(PT_STR, str);
}

struct ptnode *ptn_new_math(char *str)
{
	return __ptn_new_str(PT_MATH, str);
}

struct ptnode *ptn_new_cmd(char *str)
{
	return __ptn_new_str(PT_CMD, str);
}

struct ptnode *ptn_new_verb(char *str)
{
	return __ptn_new_str(PT_VERB, str);
}

struct ptnode *ptn_new_mchar(char ch, int len)
{
	struct ptnode *ptn;

	ptn = ptn_new(PT_CHAR);
	ASSERT(ptn);

	ptn->u.ch.ch = ch;
	ptn->u.ch.len = len;

	return ptn;
}

struct ptnode *ptn_new_opt(enum pttype type, struct ptree *tree)
{
	struct ptnode *ptn;

	ASSERT((type == PT_OPT_MAN) || (type == PT_OPT_OPT));

	ptn = ptn_new(type);
	ASSERT(ptn);

	ptn->u.tree = tree;

	return ptn;
}

struct ptnode *ptn_new_env(bool begin)
{
	struct ptnode *ptn;

	ptn = ptn_new(PT_ENV);
	ASSERT(ptn);

	ptn->u.b = begin;

	return ptn;
}

struct ptree *pt_append(struct ptree *cur, struct ptnode *new)
{
	if (!cur) {
		cur = malloc(sizeof(struct ptree));
		ASSERT(cur);

		INIT_LIST_HEAD(&cur->nodes);
	}

	if (new)
		list_add_tail(&new->list, &cur->nodes);

	return cur;
}

struct ptree *pt_extend(struct ptree *cur, struct ptree *new)
{
	if (!cur)
		return new;

	if (new)
		list_splice(&new->nodes, &cur->nodes);

	return cur;
}

static const char *pt_typename(enum pttype type)
{
	static const char *names[] = {
		[PT_STR]	= "PT_STR",
		[PT_CHAR]	= "PT_CHAR",
		[PT_NL]		= "PT_NL",
		[PT_NBSP]	= "PT_NBSP",
		[PT_MATH]	= "PT_MATH",
		[PT_VERB]	= "PT_VERB",
		[PT_CMD]	= "PT_CMD",
		[PT_ENV]	= "PT_ENV",
		[PT_PAR]	= "PT_PAR",
		[PT_OPT_MAN]	= "PT_OPT_MAN",
		[PT_OPT_OPT]	= "PT_OPT_OPT",
		[PT_TBL_COL]	= "PT_TBL_COL",
	};

	if ((type < PT_STR) || (type > PT_TBL_COL))
		return "???";

	return names[type];
}

static void __pt_dump(struct ptree *tree, int indent)
{
	struct ptnode *cur, *tmp;

	list_for_each_entry_safe(cur, tmp, &tree->nodes, list) {
		fprintf(stderr, "%*s  %p - %s\n", indent, "", cur,
			pt_typename(cur->type));

		switch (cur->type) {
			case PT_STR:
			case PT_MATH:
			case PT_CMD:
			case PT_VERB:
				fprintf(stderr, "%*s    >>%s<<\n", indent,
					"", cur->u.str);
				break;
			case PT_ENV:
				fprintf(stderr, "%*s    (%s)\n", indent,
					"", cur->u.b ? "begin" : "end");
				break;
			case PT_CHAR:
				fprintf(stderr, "%*s    '%c' * %d\n",
					indent, "", cur->u.ch.ch,
					cur->u.ch.len);
				break;
			case PT_OPT_MAN:
			case PT_OPT_OPT:
				__pt_dump(cur->u.tree, indent + 2);
				break;
			default:
				/* do nothing */
				break;
		}
	}
}

void pt_dump(struct ptree *tree)
{
	__pt_dump(tree, 0);
}

void pt_destroy(struct ptree *tree)
{
#warning memory leak
}
