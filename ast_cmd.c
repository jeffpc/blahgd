#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* NOTE: this enum must by storted! */
enum cmd_idx {
	CMD_IDX_abbrev,
	CMD_IDX_begin,
	CMD_IDX_bug,
	CMD_IDX_category,
	CMD_IDX_degree,
	CMD_IDX_emph,
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
	CMD_IDX_textbf,
	CMD_IDX_textit,
	CMD_IDX_texttt,
	CMD_IDX_title,
	CMD_IDX_tm,
	CMD_IDX_trow,
	CMD_IDX_wiki,
};

#define CMD(n, m, o)	[CMD_IDX_##n] = { .name = #n, .nmand = (m), .nopt = (o), }

static const struct ast_cmd cmds[] = {
	CMD(abbrev,			1, 1),
	CMD(begin,			1, 0),
	CMD(bug,			1, 1),
	CMD(category,			1, 0),
	CMD(degree,			1, 0),
	CMD(emph,			1, 0),
	CMD(end,			1, 0),
	CMD(img,			1, 1),
	CMD(item,			1, 0),
	CMD(leftarrow,			0, 0),
	CMD(leftrightarrow,		0, 0),
	CMD(link,			1, 1),
	CMD(listing,			1, 0),
	CMD(photo,			1, 1),
	CMD(photolink,			1, 1),
	CMD(post,			1, 1),
	CMD(published,			1, 0),
	CMD(rightarrow,			0, 0),
	CMD(section,			1, 0),
	CMD(subsection,			1, 0),
	CMD(subsubsection,		1, 0),
	CMD(tag,			1, 0),
	CMD(taglink,			1, 1),
	CMD(textbf,			1, 0),
	CMD(textit,			1, 0),
	CMD(texttt,			1, 0),
	CMD(title,			1, 0),
	CMD(tm,				0, 0),
	CMD(trow,			1, 0),
	CMD(wiki,			1, 1),
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
