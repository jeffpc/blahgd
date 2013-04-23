#include <stdbool.h>

#include "config.h"
#include "post_fmt3_cmds.h"
#include "listing.h"
#include "utils.h"

static char *table(struct post *post, char *opt, bool begin)
{
	char *a, *b;

	ASSERT(!opt);

	/* only begin/end the paragraph for the first level */

	if (begin) {
		a = post->table_nesting++ ? "" : "</p>";
		b = "<table>";
	} else {
		a = "</table>";
		b = --post->table_nesting ? "" : "<p>";
	}

	return concat(S(a), S(b));
}

static char *__process_listing(struct post *post, char *txt, char *opt)
{
	return concat3(S("</p><pre>"), listing(post, txt), S("</pre><p>"));
}

static char *__process_link(struct post *post, char *txt, char *opt)
{
	return concat5(S("<a href=\""), txt, S("\">"), opt ? opt : txt, S("</a>"));
}

static char *__process_photolink(struct post *post, char *txt, char *opt)
{
	return concat5(S("<a href=\"" PHOTO_BASE_URL "/"), txt, S("\">"), opt ? opt : txt, S("</a>"));
}

static char *__process_img(struct post *post, char *txt, char *opt)
{
	return concat5(S("<img src=\""), txt, S("\" alt=\""), opt, S("\" />"));
}

static char *__process_photo(struct post *post, char *txt, char *opt)
{
	return concat5(S("<img src=\"" PHOTO_BASE_URL "/"), txt, S("\" alt=\""), opt, S("\" />"));
}

static char *__process_emph(struct post *post, char *txt, char *opt)
{
	return concat3(S("<em>"), txt, S("</em>"));
}

static char *__process_texttt(struct post *post, char *txt, char *opt)
{
	return concat3(S("<tt>"), txt, S("</tt>"));
}

static char *__process_textbf(struct post *post, char *txt, char *opt)
{
	return concat3(S("<strong>"), txt, S("</strong>"));
}

static char *__process_textit(struct post *post, char *txt, char *opt)
{
	return concat3(S("<i>"), txt, S("</i>"));
}

static char *__process_begin(struct post *post, char *txt, char *opt)
{
	bool begin = true;

	if (!strcmp(txt, "enumerate")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><ol>" : "</ol><p>");
	}

	if (!strcmp(txt, "itemize")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><ul>" : "</ul><p>");
	}

	if (!strcmp(txt, "description")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><dl>" : "</dl><p>");
	}

	if (!strcmp(txt, "quote")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><blockquote><p>" :
				      "</p></blockquote><p>");
	}

	if (!strcmp(txt, "tabular"))
		return table(post, opt, begin);

	return concat3(S("[INVAL ENVIRON"), txt, S("]"));
}

static char *__process_end(struct post *post, char *txt, char *opt)
{
	bool begin = false;

	if (!strcmp(txt, "enumerate")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><ol>" : "</ol><p>");
	}

	if (!strcmp(txt, "itemize")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><ul>" : "</ul><p>");
	}

	if (!strcmp(txt, "description")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><dl>" : "</dl><p>");
	}

	if (!strcmp(txt, "quote")) {
		ASSERT(!opt);
		return xstrdup(begin ? "</p><blockquote><p>" :
				      "</p></blockquote><p>");
	}

	if (!strcmp(txt, "tabular"))
		return table(post, opt, begin);

	return concat3(S("[INVAL ENVIRON"), txt, S("]"));
}

static char *__process_item(struct post *post, char *txt, char *opt)
{
	// FIXME: we should keep track of what commands we've
	// encountered and then decide if <li> is the right tag to
	// use
	if (!opt)
		return concat3(S("<li>"), txt, S("</li>"));
	return concat5(S("<dt>"), opt, S("</dt><dd>"), txt, S("</dd>"));
}

static char *__process_abbrev(struct post *post, char *txt, char *opt)
{
	return concat5(S("<abbr title=\""), opt ? opt : txt, S("\">"), txt, S("</abbr>"));
}

static char *__process_section(struct post *post, char *txt, char *opt)
{
	return concat3(S("</p><h4>"), txt, S("</h4><p>"));
}

static char *__process_subsection(struct post *post, char *txt, char *opt)
{
	return concat3(S("</p><h5>"), txt, S("</h5><p>"));
}

static char *__process_subsubsection(struct post *post, char *txt, char *opt)
{
	return concat3(S("</p><h6>"), txt, S("</h6><p>"));
}

static char *__process_wiki(struct post *post, char *txt, char *opt)
{
	return concat5(S("<a href=\"" WIKI_BASE_URL "/"), txt,
		S("\"><img src=\"" BASE_URL "/wiki.png\" alt=\"Wikipedia article:\" />&nbsp;"),
		opt ? opt : txt, S("</a>"));
}

static char *__process_bug(struct post *post, char *txt, char *opt)
{
	return concat5(S("<a href=\"" BUG_BASE_URL "/"), txt,
		S("\"><img src=\"" BASE_URL "/bug.png\" alt=\"bug #\" />&nbsp;"),
		txt, S("</a>"));
}

static char *__process_degree(struct post *post, char *txt, char *opt)
{
	return concat(S("\xc2\xb0"), txt);
}

static char *__process_trow(struct post *post, char *txt, char *opt)
{
	return concat3(S("<tr><td>"), txt, S("</td></tr>"));
}

static char *__process_leftarrow(struct post *post, char *txt, char *opt)
{
	return xstrdup("&larr;");
}

static char *__process_rightarrow(struct post *post, char *txt, char *opt)
{
	return xstrdup("&rarr;");
}

static char *__process_leftrightarrow(struct post *post, char *txt, char *opt)
{
	return xstrdup("&harr;");
}

static char *__process_nop(struct post *post, char *txt, char *opt)
{
	return xstrdup("");
}

typedef enum {
	PROHIBITED,
	ALLOWED,
	REQUIRED,
} tri;

struct cmd {
	const char *name;
	char *(*fxn)(struct post *, char *txt, char *opt);
	tri square;
	tri curly;
};

int cmd_cmp(const void *va, const void *vb)
{
	const struct cmd *a = va;
	const struct cmd *b = vb;

	return strcmp(a->name, b->name);
}

#define CMD(n, f, s, c)	{ .name = #n, .fxn = f, .square = (s), .curly = (c), }
#define CMD_OPT(n)	CMD(n, __process_##n, ALLOWED, REQUIRED)
#define CMD_REQ(n)	CMD(n, __process_##n, PROHIBITED, REQUIRED)
#define CMD_NON(n)	CMD(n, __process_##n, PROHIBITED, PROHIBITED)
#define CMD_NOP(n)	CMD(n, __process_nop, PROHIBITED, REQUIRED)

/* NOTE: this array must by storted! */
static const struct cmd cmds[] = {
	CMD_OPT(abbrev),
	CMD_REQ(begin),
	CMD_REQ(bug),
	CMD_REQ(degree),
	CMD_REQ(emph),
	CMD_REQ(end),
	CMD_OPT(img),
	CMD_OPT(item),
	CMD_NON(leftarrow),
	CMD_NON(leftrightarrow),
	CMD_OPT(link),
	CMD_OPT(listing),
	CMD_OPT(photo),
	CMD_OPT(photolink),
	CMD_NON(rightarrow),
	CMD_REQ(section),
	CMD_REQ(subsection),
	CMD_REQ(subsubsection),
	CMD_NOP(tag),
	CMD_REQ(textbf),
	CMD_REQ(textit),
	CMD_REQ(texttt),
	CMD_NOP(title),
	CMD_REQ(trow),
	CMD_OPT(wiki),
};

static void __check_arg(tri r, char *ptr)
{
	if (r == REQUIRED)
		ASSERT(ptr);

	if (r == PROHIBITED)
		ASSERT(!ptr);
}

char *process_cmd(struct post *post, char *cmd, char *txt, char *opt)
{
	struct cmd key = {
		.name = cmd,
	};
	const struct cmd *c;

	c = bsearch(&key, cmds, sizeof(cmds) / sizeof(cmds[0]),
		    sizeof(cmds[0]), cmd_cmp);
	if (!c) {
		LOG("post_fmt3: invalid command '%s' (post #%u)", cmd, post->id);

		return concat3(S("[INVAL CMD"), cmd, S("]"));
	}

	__check_arg(c->square, opt);
	__check_arg(c->curly, txt);

	return c->fxn(post, txt, opt);
}
