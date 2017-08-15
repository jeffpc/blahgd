/*
 * Copyright (c) 2013-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/str.h>
#include <jeffpc/types.h>

#include "config.h"
#include "post_fmt3_cmds.h"
#include "listing.h"
#include "utils.h"
#include "debug.h"

/*
 * NOTE:
 *
 * To add a new command 'foo', simply...
 *  (1) add a CMD_IDX_foo to cmd_idx enum
 *  (2) add entry to cmds array
 *  (3) create a function __process_foo(...)
 */

static struct str *__process_listing(struct parser_output *data,
				     struct str *txt, struct str *opt)
{
	struct str *str;

	str = listing(data->post, str_cstr(txt));

	str_putref(txt);
	str_putref(opt);

	return str_cat(3, STATIC_STR("</p><pre>"), str, STATIC_STR("</pre><p>"));
}

static struct str *__process_link(struct parser_output *data, struct str *txt,
				  struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat(5, STATIC_STR("<a href=\""), txt, STATIC_STR("\">"),
		       opt ? opt : txt, STATIC_STR("</a>"));
}

static struct str *__process_photolink(struct parser_output *data,
				       struct str *txt, struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat(7, STATIC_STR("<a href=\""),
		       str_getref(config.photo_base_url),
		       STATIC_STR("/"),
		       txt,
		       STATIC_STR("\">"), opt ? opt : txt, STATIC_STR("</a>"));
}

static struct str *__process_img(struct parser_output *data, struct str *txt,
				 struct str *opt)
{
	return str_cat(5, STATIC_STR("<img src=\""), txt, STATIC_STR("\" alt=\""),
		       opt, STATIC_STR("\" />"));
}

static struct str *__process_photo(struct parser_output *data, struct str *txt,
				   struct str *opt)
{
	return str_cat(7, STATIC_STR("<img src=\""),
		       str_getref(config.photo_base_url),
		       STATIC_STR("/"),
		       txt,
		       STATIC_STR("\" alt=\""), opt, STATIC_STR("\" />"));
}

static struct str *__process_emph(struct parser_output *data, struct str *txt,
				  struct str *opt)
{
	return str_cat(3, STATIC_STR("<em>"), txt, STATIC_STR("</em>"));
}

static struct str *__process_texttt(struct parser_output *data, struct str *txt,
				    struct str *opt)
{
	return str_cat(3, STATIC_STR("<tt>"), txt, STATIC_STR("</tt>"));
}

static struct str *__process_textbf(struct parser_output *data,
				    struct str *txt, struct str *opt)
{
	return str_cat(3, STATIC_STR("<strong>"), txt, STATIC_STR("</strong>"));
}

static struct str *__process_textit(struct parser_output *data, struct str *txt,
				    struct str *opt)
{
	return str_cat(3, STATIC_STR("<i>"), txt, STATIC_STR("</i>"));
}

static struct str *__process_begin(struct parser_output *data, struct str *txt,
				   struct str *opt)
{
	if (!strcmp(str_cstr(txt), "texttt")) {
		data->texttt_nesting++;
		str_putref(txt);
		return STATIC_STR("</p><pre>");
	}

	if (!strcmp(str_cstr(txt), "enumerate")) {
		str_putref(txt);
		return STATIC_STR("</p><ol>");
	}

	if (!strcmp(str_cstr(txt), "itemize")) {
		str_putref(txt);
		return STATIC_STR("</p><ul>");
	}

	if (!strcmp(str_cstr(txt), "description")) {
		str_putref(txt);
		return STATIC_STR("</p><dl>");
	}

	if (!strcmp(str_cstr(txt), "quote")) {
		str_putref(txt);
		return STATIC_STR("</p><blockquote><p>");
	}

	if (!strcmp(str_cstr(txt), "tabular")) {
		str_putref(txt);

		if (data->table_nesting++)
			return STATIC_STR("<table>");
		return STATIC_STR("</p><table>");
	}

	DBG("post_fmt3: invalid environment '%s' (post #%u)", str_cstr(txt),
	    data->post->id);

	return str_cat(3, STATIC_STR("[INVAL ENVIRON '"), txt, STATIC_STR("']"));
}

static struct str *__process_end(struct parser_output *data, struct str *txt,
				 struct str *opt)
{
	if (!strcmp(str_cstr(txt), "texttt")) {
		data->texttt_nesting--;
		str_putref(txt);
		return STATIC_STR("</pre><p>");
	}

	if (!strcmp(str_cstr(txt), "enumerate")) {
		str_putref(txt);
		return STATIC_STR("</ol><p>");
	}

	if (!strcmp(str_cstr(txt), "itemize")) {
		str_putref(txt);
		return STATIC_STR("</ul><p>");
	}

	if (!strcmp(str_cstr(txt), "description")) {
		str_putref(txt);
		return STATIC_STR("</dl><p>");
	}

	if (!strcmp(str_cstr(txt), "quote")) {
		str_putref(txt);
		return STATIC_STR("</p></blockquote><p>");
	}

	if (!strcmp(str_cstr(txt), "tabular")) {
		str_putref(txt);

		if (--data->table_nesting)
			return STATIC_STR("</table>");
		return STATIC_STR("</table><p>");
	}

	DBG("post_fmt3: invalid environment '%s' (post #%u)", str_cstr(txt),
	    data->post->id);

	return str_cat(3, STATIC_STR("[INVAL ENVIRON '"), txt, STATIC_STR("']"));
}

static struct str *__process_item(struct parser_output *data, struct str *txt,
				  struct str *opt)
{
	// FIXME: we should keep track of what commands we've
	// encountered and then decide if <li> is the right tag to
	// use
	if (!opt)
		return str_cat(3, STATIC_STR("<li>"), txt, STATIC_STR("</li>"));
	return str_cat(5, STATIC_STR("<dt>"), opt, STATIC_STR("</dt><dd>"), txt,
		       STATIC_STR("</dd>"));
}

static struct str *__process_abbrev(struct parser_output *data, struct str *txt,
				    struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat(5, STATIC_STR("<abbr title=\""), opt ? opt : txt,
		       STATIC_STR("\">"), txt, STATIC_STR("</abbr>"));
}

static struct str *__process_section(struct parser_output *data,
				     struct str *txt, struct str *opt)
{
	return str_cat(3, STATIC_STR("</p><h3>"), txt, STATIC_STR("</h3><p>"));
}

static struct str *__process_subsection(struct parser_output *data,
					struct str *txt, struct str *opt)
{
	return str_cat(3, STATIC_STR("</p><h4>"), txt, STATIC_STR("</h4><p>"));
}

static struct str *__process_subsubsection(struct parser_output *data,
					   struct str *txt, struct str *opt)
{
	return str_cat(3, STATIC_STR("</p><h5>"), txt, STATIC_STR("</h5><p>"));
}

static struct str *__process_wiki(struct parser_output *data, struct str *txt,
				  struct str *opt)
{
	if (!opt)
		str_getref(txt);

	return str_cat(9, STATIC_STR("<a href=\""),
		       str_getref(config.wiki_base_url),
		       STATIC_STR("/"),
		       txt,
		       STATIC_STR("\"><img src=\""),
		       str_getref(config.base_url),
		       STATIC_STR("/wiki.png\" alt=\"Wikipedia article:\" />&nbsp;"),
		       opt ? opt : txt, STATIC_STR("</a>"));
}

static struct str *__process_bug(struct parser_output *data, struct str *txt,
				 struct str *opt)
{
	str_getref(txt);

	return str_cat(9, STATIC_STR("<a href=\""),
		       str_getref(config.bug_base_url),
		       STATIC_STR("/"),
		       txt,
		       STATIC_STR("\"><img src=\""),
		       str_getref(config.base_url),
		       STATIC_STR("/bug.png\" alt=\"bug #\" />&nbsp;"),
		       txt, STATIC_STR("</a>"));
}

static struct str *__process_post(struct parser_output *data, struct str *txt,
				  struct str *opt)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "<a href=\"%s/?p=%s\">%s</a>",
		 str_cstr(config.base_url), str_cstr(txt),
		 opt ? str_cstr(opt) : str_cstr(txt));

	str_putref(txt);
	str_putref(opt);

	return STR_DUP(buf);
}

static struct str *__process_taglink(struct parser_output *data,
				     struct str *txt, struct str *opt)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "<a href=\"%s/?tag=%s\">%s</a>",
		 str_cstr(config.base_url), str_cstr(txt),
		 opt ? str_cstr(opt) : str_cstr(txt));

	str_putref(txt);
	str_putref(opt);

	return STR_DUP(buf);
}

static struct str *__process_degree(struct parser_output *data, struct str *txt,
				    struct str *opt)
{
	return str_cat(2, STATIC_STR("\xc2\xb0"), txt);
}

static struct str *__process_trow(struct parser_output *data, struct str *txt,
				  struct str *opt)
{
	return str_cat(3, STATIC_STR("<tr><td>"), txt, STATIC_STR("</td></tr>"));
}

static struct str *__process_leftarrow(struct parser_output *data,
				       struct str *txt, struct str *opt)
{
	return STATIC_STR("&larr;");
}

static struct str *__process_rightarrow(struct parser_output *data,
					struct str *txt, struct str *opt)
{
	return STATIC_STR("&rarr;");
}

static struct str *__process_leftrightarrow(struct parser_output *data,
					    struct str *txt, struct str *opt)
{
	return STATIC_STR("&harr;");
}

static struct str *__process_tm(struct parser_output *data, struct str *txt,
				struct str *opt)
{
	return STATIC_STR("&trade;");
}

static struct str *__process_nop(struct parser_output *data, struct str *txt,
				 struct str *opt)
{
	str_putref(txt);
	str_putref(opt);

	return str_empty_string();
}

typedef enum {
	PROHIBITED,
	ALLOWED,
	REQUIRED,
} tri;

struct cmd {
	const char *name;
	struct str *(*fxn)(struct parser_output *, struct str *txt,
			   struct str *opt);
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

struct str *process_cmd(struct parser_output *data, struct str *cmd,
			struct str *txt, struct str *opt)
{
	struct cmd key;
	const struct cmd *c;

	key.name = str_cstr(cmd);

	c = bsearch(&key, cmds, ARRAY_LEN(cmds), sizeof(cmds[0]), cmd_cmp);
	if (!c) {
		DBG("post_fmt3: invalid command '%s' (post #%u)", key.name,
		    data->post->id);

		str_putref(txt);
		str_putref(opt);

		return str_cat(3, STATIC_STR("[INVAL CMD '"), cmd,
			       STATIC_STR("']"));
	}

	__check_arg(c->square, opt);
	__check_arg(c->curly, txt);

	str_putref(cmd);

	return c->fxn(data, txt, opt);
}
