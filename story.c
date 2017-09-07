/*
 * Copyright (c) 2009-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/error.h>

#include "post.h"
#include "req.h"
#include "render.h"
#include "sidebar.h"
#include "utils.h"
#include "debug.h"

static int __load_post(struct req *req, int p, bool preview)
{
	struct nvlist *post;
	struct nvval *val;

	post = get_post(req, p, "title", preview);
	if (!post) {
		DBG("failed to load post #%d: %s (%d)%s", p, "XXX",
		    -1, preview ? " preview" : "");

		vars_set_str(&req->vars, "title", STATIC_STR("not found"));

		return -ENOENT;
	}

	val = malloc(sizeof(struct nvval));
	if (!val)
		return -ENOMEM;

	val->type = NVT_NVL;
	val->nvl = post;

	vars_set_array(&req->vars, "posts", val, 1);

	return 0;
}

/*
 * Is the request a preview?
 */
static bool is_preview(struct req *req)
{
	uint64_t tmp;

	if (nvl_lookup_int(req->scgi->request.query, "preview", &tmp))
		return false;

	return tmp == PREVIEW_SECRET;
}

int blahg_story(struct req *req)
{
	uint64_t postid;

	if (nvl_lookup_int(req->scgi->request.query, "p", &postid))
		return R404(req, NULL);
	if (postid > INT_MAX)
		return R404(req, NULL);

	sidebar(req);

	vars_scope_push(&req->vars);

	if (__load_post(req, postid, is_preview(req)))
		return R404(req, NULL);

	req->scgi->response.body = render_page(req, "{storyview}");

	return 0;
}
