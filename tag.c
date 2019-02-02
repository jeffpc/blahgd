/*
 * Copyright (c) 2011-2019 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/array.h>
#include <jeffpc/error.h>
#include <jeffpc/types.h>
#include <jeffpc/sexpr.h>

#include "config.h"
#include "render.h"
#include "req.h"
#include "utils.h"
#include "sidebar.h"
#include "post.h"

static void __store_title(struct vars *vars, struct str *title)
{
	char twittertitle[1024];

	snprintf(twittertitle, sizeof(twittertitle), "%s » %s", "Blahg",
		 str_cstr(title));

	vars_set_str(vars, "title", title);
	vars_set_str(vars, "twittertitle", STR_DUP(twittertitle));
}

static void __store_tag(struct vars *vars, struct str *tag)
{
	vars_set_str(vars, "tagid", tag);
}

static void __store_pages(struct vars *vars, int page)
{
	vars_set_int(vars, "prevpage", page + 1);
	vars_set_int(vars, "curpage",  page);
	vars_set_int(vars, "nextpage", page - 1);
}

int blahg_tag(struct req *req, int page)
{
	const unsigned int posts_per_page = req->opts.index_stories;
	struct post *posts[posts_per_page];
	struct str *tag;
	int nposts;

	tag = nvl_lookup_str(req->scgi->request.query, "tag");
	if (IS_ERR(tag))
		return R404(req, NULL);

	req_head(req, "Content-Type", "text/html");

	__store_title(&req->vars, str_getref(tag));
	__store_pages(&req->vars, page);
	__store_tag(&req->vars, str_getref(tag));

	sidebar(req);

	vars_scope_push(&req->vars);

	nposts = index_get_posts(posts, tag, NULL, NULL,
				 page * posts_per_page, posts_per_page);

	load_posts(req, posts, nposts, nposts == posts_per_page);

	str_putref(tag);

	req->scgi->response.body = render_page(req, "{tagindex}");

	return 0;
}

/*
 * This is the simplest way to handle categories without needing to
 * supporting them anymore.  We simply map the category to a tag based on
 * what's in the category-to-tag config alist.
 *
 * If the category doesn't exist in the alist, we return a 404.  Otherwise,
 * we use a 301 redirect to the tag page.
 */
int blahg_category(struct req *req, int page)
{
	struct str *tag;
	struct str *cat;

	cat = nvl_lookup_str(req->scgi->request.query, "cat");
	if (IS_ERR(cat))
		return R404(req, NULL);

	tag = sexpr_alist_lookup_str(config.category_to_tag, str_cstr(cat));

	str_putref(cat);

	if (!tag)
		return R404(req, NULL);

	return R301(req,
		    str_cat(3,
			    str_getref(config.base_url),
			    STATIC_STR("/?tag="),
			    tag));
}
