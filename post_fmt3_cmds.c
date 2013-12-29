#include <stdbool.h>

#include "config.h"
#include "post_fmt3_cmds.h"
#include "listing.h"
#include "utils.h"

/*
 * NOTE:
 *
 * To add a new command 'foo', simply...
 *  (1) add a CMD_IDX_foo to cmd_idx enum
 *  (2) add entry to cmds array
 *  (3) create a function __process_foo(...)
 */

static struct val *__process_listing(struct post *post, struct val *txt, struct val *opt)
{
	struct val *val;

	val = listing(post, txt->str);

	val_putref(txt);
	val_putref(opt);

	return valcat3(VAL_DUP_STR("</p><pre>"), val, VAL_DUP_STR("</pre><p>"));
}

static struct val *__process_link(struct post *post, struct val *txt, struct val *opt)
{
	if (!opt)
		val_getref(txt);

	return valcat5(VAL_DUP_STR("<a href=\""), txt, VAL_DUP_STR("\">"),
		       opt ? opt : txt, VAL_DUP_STR("</a>"));
}

static struct val *__process_photolink(struct post *post, struct val *txt, struct val *opt)
{
	if (!opt)
		val_getref(txt);

	return valcat5(VAL_DUP_STR("<a href=\"" PHOTO_BASE_URL "/"), txt,
		       VAL_DUP_STR("\">"), opt ? opt : txt, VAL_DUP_STR("</a>"));
}

static struct val *__process_img(struct post *post, struct val *txt, struct val *opt)
{
	return valcat5(VAL_DUP_STR("<img src=\""), txt, VAL_DUP_STR("\" alt=\""),
		       opt, VAL_DUP_STR("\" />"));
}

static struct val *__process_photo(struct post *post, struct val *txt, struct val *opt)
{
	return valcat5(VAL_DUP_STR("<img src=\"" PHOTO_BASE_URL "/"), txt,
		       VAL_DUP_STR("\" alt=\""), opt, VAL_DUP_STR("\" />"));
}

static struct val *__process_emph(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("<em>"), txt, VAL_DUP_STR("</em>"));
}

static struct val *__process_texttt(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("<tt>"), txt, VAL_DUP_STR("</tt>"));
}

static struct val *__process_textbf(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("<strong>"), txt, VAL_DUP_STR("</strong>"));
}

static struct val *__process_textit(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("<i>"), txt, VAL_DUP_STR("</i>"));
}

static struct val *__process_begin(struct post *post, struct val *txt, struct val *opt)
{
	if (!strcmp(txt->str, "texttt")) {
		post->texttt_nesting++;
		val_putref(txt);
		return VAL_DUP_STR("</p><pre>");
	}

	if (!strcmp(txt->str, "enumerate")) {
		val_putref(txt);
		return VAL_DUP_STR("</p><ol>");
	}

	if (!strcmp(txt->str, "itemize")) {
		val_putref(txt);
		return VAL_DUP_STR("</p><ul>");
	}

	if (!strcmp(txt->str, "description")) {
		val_putref(txt);
		return VAL_DUP_STR("</p><dl>");
	}

	if (!strcmp(txt->str, "quote")) {
		val_putref(txt);
		return VAL_DUP_STR("</p><blockquote><p>");
	}

	if (!strcmp(txt->str, "tabular")) {
		val_putref(txt);

		if (post->table_nesting++)
			return VAL_DUP_STR("<table>");
		return VAL_DUP_STR("</p><table>");
	}

	LOG("post_fmt3: invalid environment '%s' (post #%u)", txt->str, post->id);

	return valcat3(VAL_DUP_STR("[INVAL ENVIRON '"), txt, VAL_DUP_STR("']"));
}

static struct val *__process_end(struct post *post, struct val *txt, struct val *opt)
{
	if (!strcmp(txt->str, "texttt")) {
		post->texttt_nesting--;
		val_putref(txt);
		return VAL_DUP_STR("</pre><p>");
	}

	if (!strcmp(txt->str, "enumerate")) {
		val_putref(txt);
		return VAL_DUP_STR("</ol><p>");
	}

	if (!strcmp(txt->str, "itemize")) {
		val_putref(txt);
		return VAL_DUP_STR("</ul><p>");
	}

	if (!strcmp(txt->str, "description")) {
		val_putref(txt);
		return VAL_DUP_STR("</dl><p>");
	}

	if (!strcmp(txt->str, "quote")) {
		val_putref(txt);
		return VAL_DUP_STR("</p></blockquote><p>");
	}

	if (!strcmp(txt->str, "tabular")) {
		val_putref(txt);

		if (--post->table_nesting)
			return VAL_DUP_STR("</table>");
		return VAL_DUP_STR("</table><p>");
	}

	LOG("post_fmt3: invalid environment '%s' (post #%u)", txt->str, post->id);

	return valcat3(VAL_DUP_STR("[INVAL ENVIRON '"), txt, VAL_DUP_STR("']"));
}

static struct val *__process_item(struct post *post, struct val *txt, struct val *opt)
{
	// FIXME: we should keep track of what commands we've
	// encountered and then decide if <li> is the right tag to
	// use
	if (!opt)
		return valcat3(VAL_DUP_STR("<li>"), txt, VAL_DUP_STR("</li>"));
	return valcat5(VAL_DUP_STR("<dt>"), opt, VAL_DUP_STR("</dt><dd>"), txt,
		       VAL_DUP_STR("</dd>"));
}

static struct val *__process_abbrev(struct post *post, struct val *txt, struct val *opt)
{
	if (!opt)
		val_getref(txt);

	return valcat5(VAL_DUP_STR("<abbr title=\""), opt ? opt : txt,
		       VAL_DUP_STR("\">"), txt, VAL_DUP_STR("</abbr>"));
}

static struct val *__process_section(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("</p><h3>"), txt, VAL_DUP_STR("</h3><p>"));
}

static struct val *__process_subsection(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("</p><h4>"), txt, VAL_DUP_STR("</h4><p>"));
}

static struct val *__process_subsubsection(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("</p><h5>"), txt, VAL_DUP_STR("</h5><p>"));
}

static struct val *__process_wiki(struct post *post, struct val *txt, struct val *opt)
{
	if (!opt)
		val_getref(txt);

	return valcat5(VAL_DUP_STR("<a href=\"" WIKI_BASE_URL "/"), txt,
		VAL_DUP_STR("\"><img src=\"" BASE_URL "/wiki.png\" alt=\"Wikipedia article:\" />&nbsp;"),
		opt ? opt : txt, VAL_DUP_STR("</a>"));
}

static struct val *__process_bug(struct post *post, struct val *txt, struct val *opt)
{
	val_getref(txt);

	return valcat5(VAL_DUP_STR("<a href=\"" BUG_BASE_URL "/"), txt,
		VAL_DUP_STR("\"><img src=\"" BASE_URL "/bug.png\" alt=\"bug #\" />&nbsp;"),
		txt, VAL_DUP_STR("</a>"));
}

static struct val *__process_post(struct post *post, struct val *txt, struct val *opt)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "<a href=\"/?p=%s\">%s</a>",
		 txt->str, opt ? opt->str : txt->str);

	val_putref(txt);
	val_putref(opt);

	return VAL_DUP_STR(buf);
}

static struct val *__process_taglink(struct post *post, struct val *txt, struct val *opt)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "<a href=\"/?tag=%s\">%s</a>",
		 txt->str, opt ? opt->str : txt->str);

	val_putref(txt);
	val_putref(opt);

	return VAL_DUP_STR(buf);
}

static struct val *__process_degree(struct post *post, struct val *txt, struct val *opt)
{
	return valcat(VAL_DUP_STR("\xc2\xb0"), txt);
}

static struct val *__process_trow(struct post *post, struct val *txt, struct val *opt)
{
	return valcat3(VAL_DUP_STR("<tr><td>"), txt, VAL_DUP_STR("</td></tr>"));
}

static struct val *__process_leftarrow(struct post *post, struct val *txt, struct val *opt)
{
	return VAL_DUP_STR("&larr;");
}

static struct val *__process_rightarrow(struct post *post, struct val *txt, struct val *opt)
{
	return VAL_DUP_STR("&rarr;");
}

static struct val *__process_leftrightarrow(struct post *post, struct val *txt, struct val *opt)
{
	return VAL_DUP_STR("&harr;");
}

static struct val *__process_tm(struct post *post, struct val *txt, struct val *opt)
{
	return VAL_DUP_STR("&trade;");
}

static struct val *__process_nop(struct post *post, struct val *txt, struct val *opt)
{
	val_putref(txt);
	val_putref(opt);

	return VAL_DUP_STR("");
}

typedef enum {
	PROHIBITED,
	ALLOWED,
	REQUIRED,
} tri;

struct cmd {
	const char *name;
	struct val *(*fxn)(struct post *, struct val *txt, struct val *opt);
	tri square;
	tri curly;
};

int cmd_cmp(const void *va, const void *vb)
{
	const struct cmd *a = va;
	const struct cmd *b = vb;

	return strcmp(a->name, b->name);
}

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

#define CMD(n, f, s, c)	{ .name = #n, .fxn = f, .square = (s), .curly = (c), }
#define CMD_OPT(n) [CMD_IDX_##n] = CMD(n, __process_##n, ALLOWED, REQUIRED)
#define CMD_REQ(n) [CMD_IDX_##n] = CMD(n, __process_##n, PROHIBITED, REQUIRED)
#define CMD_NON(n) [CMD_IDX_##n] = CMD(n, __process_##n, PROHIBITED, PROHIBITED)
#define CMD_NOP(n) [CMD_IDX_##n] = CMD(n, __process_nop, PROHIBITED, REQUIRED)

static const struct cmd cmds[] = {
	CMD_NON(leftarrow),
	CMD_NON(leftrightarrow),
	CMD_NON(rightarrow),
	CMD_NON(tm),
	CMD_NOP(category),
	CMD_NOP(tag),
	CMD_NOP(title),
	CMD_NOP(published),
	CMD_OPT(abbrev),
	CMD_OPT(img),
	CMD_OPT(item),
	CMD_OPT(link),
	CMD_OPT(listing),
	CMD_OPT(photo),
	CMD_OPT(photolink),
	CMD_OPT(post),
	CMD_OPT(taglink),
	CMD_OPT(wiki),
	CMD_REQ(begin),
	CMD_REQ(bug),
	CMD_REQ(degree),
	CMD_REQ(emph),
	CMD_REQ(end),
	CMD_REQ(section),
	CMD_REQ(subsection),
	CMD_REQ(subsubsection),
	CMD_REQ(textbf),
	CMD_REQ(textit),
	CMD_REQ(texttt),
	CMD_REQ(trow),
};

static void __check_arg(tri r, struct val *ptr)
{
	if (r == REQUIRED)
		ASSERT(ptr);

	if (r == PROHIBITED)
		ASSERT(!ptr);
}

struct val *process_cmd(struct post *post, struct val *cmd, struct val *txt,
			struct val *opt)
{
	struct cmd key;
	const struct cmd *c;

	ASSERT3U(cmd->type, ==, VT_STR);

	if (txt)
		ASSERT3U(txt->type, ==, VT_STR);

	if (opt)
		ASSERT3U(opt->type, ==, VT_STR);

	key.name = cmd->str;

	c = bsearch(&key, cmds, ARRAY_LEN(cmds), sizeof(cmds[0]), cmd_cmp);
	if (!c) {
		LOG("post_fmt3: invalid command '%s' (post #%u)", cmd->str, post->id);

		val_putref(txt);
		val_putref(opt);

		return valcat3(VAL_DUP_STR("[INVAL CMD '"), cmd,
			       VAL_DUP_STR("']"));
	}

	__check_arg(c->square, opt);
	__check_arg(c->curly, txt);

	val_putref(cmd);

	return c->fxn(post, txt, opt);
}
