/*
 * Copyright (c) 2011-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

/*
 * Wordpress uses category numbers.  We need to map them to the string based
 * category names we use and generate a redirect.
 *
 * The list of number to string mappings is in the config file and we stuff
 * them into these variables on startup.  See init_wordpress_categories().
 */
static struct str **wordpress_cats;

static void __store_title(struct vars *vars, const char *title)
{
	char twittertitle[1024];

	snprintf(twittertitle, sizeof(twittertitle), "%s » %s", "Blahg", title);

	vars_set_str(vars, "title", STR_DUP(title));
	vars_set_str(vars, "twittertitle", STR_DUP(twittertitle));
}

static void __store_tag(struct vars *vars, const char *tag)
{
	vars_set_str(vars, "tagid", STR_DUP(tag));
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

	req->scgi->response.body = render_page(req, tmpl);

	return 0;
}

int blahg_tag(struct req *req, const char *tag, int page)
{
	return __tagcat(req, tag, page, "{tagindex}", true);
}

int blahg_category(struct req *req, const char *cat, int page)
{
	uint32_t catn;

	/*
	 * If we fail to parse the category number or it refers to a
	 * non-mapped category, we just use it as is.
	 */

	if (wordpress_cats && !str2u32(cat, &catn)) {
		char url[256];

		if (catn >= array_size(wordpress_cats))
			goto out;

		if (!wordpress_cats[catn])
			goto out;

		snprintf(url, sizeof(url), "%s/?cat=%s",
			 str_cstr(config.base_url),
			 str_cstr(wordpress_cats[catn]));

		return R301(req, url);
	}

out:
	return __tagcat(req, cat, page, "{catindex}", false);
}

/*
 * Each entry should be of the form: (int . str)
 */
static int store_wordpress_category(struct val *cur)
{
	struct val *idx, *name;
	int ret;

	if (!cur)
		return 0; /* empty list is ok */

	if (cur->type != VT_CONS)
		return -EINVAL;

	idx = sexpr_car(val_getref(cur));
	name = sexpr_cdr(cur);

	if ((idx->type != VT_INT) || (name->type != VT_STR)) {
		cmn_err(CE_ERROR, "wordpress category entry type error - "
			"expecting: (int . string)");
		ret = -EINVAL;
		goto out;
	}

	if (idx->i >= array_size(wordpress_cats)) {
		ret = array_truncate(&wordpress_cats, idx->i + 1);
		if (ret)
			goto out;
	}

	wordpress_cats[idx->i] = val_getref_str(name);

	ret = 0;

out:
	val_putref(idx);
	val_putref(name);

	return ret;
}

int init_wordpress_categories(void)
{
	struct val *cur, *tmp;
	int ret = 0;

	wordpress_cats = array_alloc(sizeof(struct str *), 0);

	sexpr_for_each_noref(cur, tmp, config.wordpress_categories) {
		ret = store_wordpress_category(val_getref(cur));
		if (ret)
			break;
	}

	/* if there is nothing, free it */
	if (!array_size(wordpress_cats)) {
		array_free(wordpress_cats);
		wordpress_cats = NULL;
	}

	return ret;
}
