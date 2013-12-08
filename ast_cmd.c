#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* NOTE: this enum must by storted! */
enum cmd_idx {
#if 0
	CMD_IDX_abbrev,
	CMD_IDX_begin,
	CMD_IDX_bug,
	CMD_IDX_category,
	CMD_IDX_degree,
#endif
	CMD_IDX_emph,
#if 0
	CMD_IDX_end,
	CMD_IDX_img,
	CMD_IDX_item,
	CMD_IDX_leftarrow,
	CMD_IDX_leftrightarrow,
	CMD_IDX_link,
	CMD_IDX_listing,
	CMD_IDX_photo,
	CMD_IDX_photolink,
	CMD_IDX_post,
	CMD_IDX_published,
	CMD_IDX_rightarrow,
	CMD_IDX_section,
	CMD_IDX_subsection,
	CMD_IDX_subsubsection,
	CMD_IDX_tag,
	CMD_IDX_taglink,
#endif
	CMD_IDX_textbf,
	CMD_IDX_textit,
	CMD_IDX_texttt,
#if 0
	CMD_IDX_title,
	CMD_IDX_tm,
	CMD_IDX_trow,
	CMD_IDX_wiki,
#endif
};

#define CMD(n, m, o)	[CMD_IDX_##n] = { .name = #n, .nmand = (m), .nopt = (o), }

static const struct ast_cmd cmds[] = {
	CMD(emph,	1, 0),
	CMD(textbf,	1, 0),
	CMD(textit,	1, 0),
	CMD(texttt,	1, 0),
};

static int cmd_cmp(const void *va, const void *vb)
{
	const struct ast_cmd *a = va;
	const struct ast_cmd *b = vb;

	return strcmp(a->name, b->name);
}

const struct ast_cmd *ast_cmd_lookup(const char *name)
{
	struct ast_cmd key = {
		.name = name,
	};

	return bsearch(&key, cmds, sizeof(cmds) / sizeof(cmds[0]),
		       sizeof(cmds[0]), cmd_cmp);
}
