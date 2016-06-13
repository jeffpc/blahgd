/*
 * Copyright (c) 2011-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include <jeffpc/error.h>

#include "config.h"
#include "render.h"
#include "req.h"
#include "utils.h"
#include "sidebar.h"
#include "post.h"

static const char *wordpress_catn[] = {
	[1]  = "miscellaneous",
	[2]  = "programming/kernel",
	[3]  = "school",
	[4]  = "work",
	[5]  = "random",
	[6]  = "programming",
	[7]  = "events/ols-2005",
	[8]  = "events",
	[9]  = "rants",
	[10] = "movies",
	[11] = "humor",
	[13] = "star-trek",
	[15] = "star-trek/tng",
	[16] = "star-trek/tos",
	[17] = "legal",
	[18] = "star-trek/voy",
	[19] = "star-trek/ent",
	[20] = "events/ols-2006",
	[21] = "fsl",
	[22] = "fsl/unionfs",
	[23] = "stargate/sg-1",
	[24] = "open-source",
	[25] = "astronomy",
	[26] = "programming/vcs",
	[27] = "programming/vcs/git",
	[28] = "programming/vcs/mercurial",
	[29] = "events/ols-2007",
	[30] = "programming/vcs/guilt",
	[31] = "photography",
	[34] = "music",
	[35] = "programming/mainframes",
	[36] = "events/sc-07",
	[39] = "hvf",
	[40] = "events/ols-2008",
	[41] = "sysadmin",
	[42] = "documentation",
	[43] = "stargate",
};

#define MIN_CATN 1
#define MAX_CATN 43

static void __store_title(struct vars *vars, const char *title)
{
	char twittertitle[1024];

	snprintf(twittertitle, sizeof(twittertitle), "%s » %s", "Blahg", title);

	vars_set_str(vars, "title", title);
	vars_set_str(vars, "twittertitle", twittertitle);
}

static void __store_tag(struct vars *vars, const char *tag)
{
	vars_set_str(vars, "tagid", tag);
}

static void __store_pages(struct vars *vars, int page)
{
	vars_set_int(vars, "prevpage", page + 1);
	vars_set_int(vars, "curpage",  page);
	vars_set_int(vars, "nextpage", page - 1);
}

int __tagcat(struct req *req, const char *tagcat, int page, char *tmpl,
	     bool istag)
{
	const unsigned int posts_per_page = req->opts.index_stories;
	struct post *posts[posts_per_page];
	struct str *tag;
	int nposts;

	if (!tagcat)
		return R404(req, NULL);

	tag = STR_DUP(tagcat);

	req_head(req, "Content-Type", "text/html");

	page = MAX(page, 0);

	__store_title(&req->vars, tagcat);
	__store_pages(&req->vars, page);
	__store_tag(&req->vars, tagcat);

	sidebar(req);

	vars_scope_push(&req->vars);

	nposts = index_get_posts(posts, tag, istag, NULL, NULL,
				 page * posts_per_page, posts_per_page);

	load_posts(req, posts, nposts, nposts == posts_per_page);

	str_putref(tag);

	req->body = render_page(req, tmpl);
	return 0;
}

int blahg_tag(struct req *req, char *tag, int page)
{
	return __tagcat(req, tag, page, "{tagindex}", true);
}

int blahg_category(struct req *req, char *cat, int page)
{
	int catn;

	/* wordpress cat name */
	catn = atoi(cat);
	if (catn && ((catn < MIN_CATN) || (catn > MAX_CATN)))
		return R404(req, NULL);

	if (catn) {
		char url[256];

		snprintf(url, sizeof(url), "%s/?cat=%s",
			 str_cstr(config.base_url), wordpress_catn[catn]);

		return R301(req, url);
	}

	return __tagcat(req, cat, page, "{catindex}", false);
}
