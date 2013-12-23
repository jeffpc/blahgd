#ifndef __PTREE_H
#define __PTREE_H

#include <stdbool.h>

#include "list.h"

enum pttype {
	PT_STR = 1,	/* must be first, >= 0 */
	PT_CHAR,
	PT_NL,
	PT_NBSP,
	PT_MATH,
	PT_VERB,
	PT_CMD,
	PT_ENV,
	PT_PAR,
	PT_OPT_MAN,
	PT_OPT_OPT,
	PT_TBL_COL,	/* must be last */
};

struct ptree {
	struct list_head nodes;
};

struct ptnode {
	struct list_head list;

	enum pttype type;
	union {
		struct {
			char *name;
			bool begin;
		} env;
		char *str;
		struct {
			char ch;
			int len;
		} ch;
		struct ptree *tree;
	} u;
};

extern struct ptree *pt_append(struct ptree *, struct ptnode *);
extern struct ptree *pt_extend(struct ptree *, struct ptree *);
extern void pt_dump(struct ptree *tree);
extern void pt_destroy(struct ptree *tree);

extern struct ptnode *ptn_new(enum pttype);
extern struct ptnode *ptn_new_str(char *);
extern struct ptnode *ptn_new_mchar(char, int);
extern struct ptnode *ptn_new_math(char *);
extern struct ptnode *ptn_new_verb(char *);
extern struct ptnode *ptn_new_cmd(char *);
extern struct ptnode *ptn_new_env(bool begin, char *name);
extern struct ptnode *ptn_new_opt(enum pttype, struct ptree *);

#define ptn_new_ws(x)		ptn_new_str(x)
#define ptn_new_char(x)		ptn_new_mchar((x), 1)
#define ptn_new_ellipsis()	ptn_new_str(xstrdup("…"))
#define ptn_new_nl()		ptn_new(PT_NL)
#define ptn_new_nbsp()		ptn_new(PT_NBSP)
#define ptn_new_par()		ptn_new(PT_PAR)

#endif
