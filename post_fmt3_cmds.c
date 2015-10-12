/*
 * Copyright (c) 2013-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdbool.h>

#include "config.h"
#include "post_fmt3_cmds.h"
#include "listing.h"
#include "utils.h"
#include "str.h"

/*
 * NOTE:
 *
 * To add a new command 'foo', simply...
 *  (1) add a CMD_IDX_foo to cmd_idx enum
 *  (2) add entry to cmds array
 *  (3) create a function __process_foo(...)
 */

static struct str *__process_listing(struct post *post, struct str *txt, struct str *opt)
{
	struct str *str;

	str = listing(post, str_cstr(txt));

	str_putref(txt);
	str_putref(opt);

	return str_cat3(STR_DUP("</p><pre>"), str, STR_DUP("</pre><p>"));
}

static struct str *__process_link(struct post *post, struct str *txt, struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat5(STR_DUP("<a href=\""), txt, STR_DUP("\">"),
		        opt ? opt : txt, STR_DUP("</a>"));
}

static struct str *__process_photolink(struct post *post, struct str *txt, struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat5(str_cat3(STR_DUP("<a href=\""),
				 str_getref(config.photo_base_url),
				 STR_DUP("/")),
			txt,
		        STR_DUP("\">"), opt ? opt : txt, STR_DUP("</a>"));
}

static struct str *__process_img(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat5(STR_DUP("<img src=\""), txt, STR_DUP("\" alt=\""),
		        opt, STR_DUP("\" />"));
}

static struct str *__process_photo(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat5(str_cat3(STR_DUP("<img src=\""),
				 str_getref(config.photo_base_url),
				 STR_DUP("/")),
			txt,
		        STR_DUP("\" alt=\""), opt, STR_DUP("\" />"));
}

static struct str *__process_emph(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("<em>"), txt, STR_DUP("</em>"));
}

static struct str *__process_texttt(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("<tt>"), txt, STR_DUP("</tt>"));
}

static struct str *__process_textbf(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("<strong>"), txt, STR_DUP("</strong>"));
}

static struct str *__process_textit(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("<i>"), txt, STR_DUP("</i>"));
}

static struct str *__process_begin(struct post *post, struct str *txt, struct str *opt)
{
	if (!strcmp(str_cstr(txt), "texttt")) {
		post->texttt_nesting++;
		str_putref(txt);
		return STR_DUP("</p><pre>");
	}

	if (!strcmp(str_cstr(txt), "enumerate")) {
		str_putref(txt);
		return STR_DUP("</p><ol>");
	}

	if (!strcmp(str_cstr(txt), "itemize")) {
		str_putref(txt);
		return STR_DUP("</p><ul>");
	}

	if (!strcmp(str_cstr(txt), "description")) {
		str_putref(txt);
		return STR_DUP("</p><dl>");
	}

	if (!strcmp(str_cstr(txt), "quote")) {
		str_putref(txt);
		return STR_DUP("</p><blockquote><p>");
	}

	if (!strcmp(str_cstr(txt), "tabular")) {
		str_putref(txt);

		if (post->table_nesting++)
			return STR_DUP("<table>");
		return STR_DUP("</p><table>");
	}

	LOG("post_fmt3: invalid environment '%s' (post #%u)", str_cstr(txt), post->id);

	return str_cat3(STR_DUP("[INVAL ENVIRON '"), txt, STR_DUP("']"));
}

static struct str *__process_end(struct post *post, struct str *txt, struct str *opt)
{
	if (!strcmp(str_cstr(txt), "texttt")) {
		post->texttt_nesting--;
		str_putref(txt);
		return STR_DUP("</pre><p>");
	}

	if (!strcmp(str_cstr(txt), "enumerate")) {
		str_putref(txt);
		return STR_DUP("</ol><p>");
	}

	if (!strcmp(str_cstr(txt), "itemize")) {
		str_putref(txt);
		return STR_DUP("</ul><p>");
	}

	if (!strcmp(str_cstr(txt), "description")) {
		str_putref(txt);
		return STR_DUP("</dl><p>");
	}

	if (!strcmp(str_cstr(txt), "quote")) {
		str_putref(txt);
		return STR_DUP("</p></blockquote><p>");
	}

	if (!strcmp(str_cstr(txt), "tabular")) {
		str_putref(txt);

		if (--post->table_nesting)
			return STR_DUP("</table>");
		return STR_DUP("</table><p>");
	}

	LOG("post_fmt3: invalid environment '%s' (post #%u)", str_cstr(txt), post->id);

	return str_cat3(STR_DUP("[INVAL ENVIRON '"), txt, STR_DUP("']"));
}

static struct str *__process_item(struct post *post, struct str *txt, struct str *opt)
{
	// FIXME: we should keep track of what commands we've
	// encountered and then decide if <li> is the right tag to
	// use
	if (!opt)
		return str_cat3(STR_DUP("<li>"), txt, STR_DUP("</li>"));
	return str_cat5(STR_DUP("<dt>"), opt, STR_DUP("</dt><dd>"), txt,
		        STR_DUP("</dd>"));
}

static struct str *__process_abbrev(struct post *post, struct str *txt, struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat5(STR_DUP("<abbr title=\""), opt ? opt : txt,
		        STR_DUP("\">"), txt, STR_DUP("</abbr>"));
}

static struct str *__process_section(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("</p><h3>"), txt, STR_DUP("</h3><p>"));
}

static struct str *__process_subsection(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("</p><h4>"), txt, STR_DUP("</h4><p>"));
}

static struct str *__process_subsubsection(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("</p><h5>"), txt, STR_DUP("</h5><p>"));
}

static struct str *__process_wiki(struct post *post, struct str *txt, struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat5(str_cat3(STR_DUP("<a href=\""),
				 str_getref(config.wiki_base_url),
				 STR_DUP("/")),
			txt,
			str_cat3(STR_DUP("\"><img src=\""),
				 str_getref(config.base_url),
				 STR_DUP("/wiki.png\" alt=\"Wikipedia article:\" />&nbsp;")),
			opt ? opt : txt, STR_DUP("</a>"));
}

static struct str *__process_bug(struct post *post, struct str *txt, struct str *opt)
{
	str_getref(txt);

	return str_cat5(str_cat3(STR_DUP("<a href=\""),
				 str_getref(config.bug_base_url),
				 STR_DUP("/")),
			txt,
			str_cat3(STR_DUP("\"><img src=\""),
				 str_getref(config.base_url),
				 STR_DUP("/bug.png\" alt=\"bug #\" />&nbsp;")),
			txt, STR_DUP("</a>"));
}

static struct str *__process_post(struct post *post, struct str *txt, struct str *opt)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "<a href=\"%s/?p=%s\">%s</a>",
		 str_cstr(config.base_url), str_cstr(txt),
		 opt ? str_cstr(opt) : str_cstr(txt));

	str_putref(txt);
	str_putref(opt);

	return STR_DUP(buf);
}

static struct str *__process_taglink(struct post *post, struct str *txt, struct str *opt)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "<a href=\"%s/?tag=%s\">%s</a>",
		 str_cstr(config.base_url), str_cstr(txt),
		 opt ? str_cstr(opt) : str_cstr(txt));

	str_putref(txt);
	str_putref(opt);

	return STR_DUP(buf);
}

static struct str *__process_degree(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat(STR_DUP("\xc2\xb0"), txt);
}

static struct str *__process_trow(struct post *post, struct str *txt, struct str *opt)
{
	return str_cat3(STR_DUP("<tr><td>"), txt, STR_DUP("</td></tr>"));
}

static struct str *__process_leftarrow(struct post *post, struct str *txt, struct str *opt)
{
	return STR_DUP("&larr;");
}

static struct str *__process_rightarrow(struct post *post, struct str *txt, struct str *opt)
{
	return STR_DUP("&rarr;");
}

static struct str *__process_leftrightarrow(struct post *post, struct str *txt, struct str *opt)
{
	return STR_DUP("&harr;");
}

static struct str *__process_tm(struct post *post, struct str *txt, struct str *opt)
{
	return STR_DUP("&trade;");
}

static struct str *__process_nop(struct post *post, struct str *txt, struct str *opt)
{
	str_putref(txt);
	str_putref(opt);

	return STR_DUP("");
}

typedef enum {
	PROHIBITED,
	ALLOWED,
	REQUIRED,
} tri;

struct cmd {
	const char *name;
	struct str *(*fxn)(struct post *, struct str *txt, struct str *opt);
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

static void __check_arg(tri r, struct str *ptr)
{
	if (r == REQUIRED)
		ASSERT(ptr);

	if (r == PROHIBITED)
		ASSERT(!ptr);
}

struct str *process_cmd(struct post *post, struct str *cmd, struct str *txt,
			struct str *opt)
{
	struct cmd key;
	const struct cmd *c;

	key.name = cmd->str;

	c = bsearch(&key, cmds, ARRAY_LEN(cmds), sizeof(cmds[0]), cmd_cmp);
	if (!c) {
		LOG("post_fmt3: invalid command '%s' (post #%u)", cmd->str, post->id);

		str_putref(txt);
		str_putref(opt);

		return str_cat3(STR_DUP("[INVAL CMD '"), cmd,
			        STR_DUP("']"));
	}

	__check_arg(c->square, opt);
	__check_arg(c->curly, txt);

	str_putref(cmd);

	return c->fxn(post, txt, opt);
}
