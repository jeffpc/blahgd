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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/sysmacros.h>

#include <jeffpc/error.h>
#include <jeffpc/mem.h>

#include "req.h"
#include "vars.h"
#include "sidebar.h"
#include "utils.h"
#include "post.h"

struct tagcloud_state {
	unsigned long ntags;
	struct nvval *cloud;
};

static int __tag_size(int count, int cmin, int cmax)
{
	float size;

	if (count <= cmin)
		return config.tagcloud_min_size;

	size  = (config.tagcloud_max_size - config.tagcloud_min_size) *
		(count - cmin);
	size /= (float) (cmax - cmin);

	return ceil(config.tagcloud_min_size + size);
}

static int __tagcloud_init(void *arg, unsigned long ntags)
{
	struct tagcloud_state *state = arg;

	state->cloud = mem_reallocarray(NULL, ntags, sizeof(struct nvval));
	state->ntags = 0;

	return state->cloud ? 0 : -ENOMEM;
}

static void __tagcloud_step(void *arg, struct str *name,
			    unsigned long count,
			    unsigned long cmin,
			    unsigned long cmax)
{
	struct tagcloud_state *state = arg;
	struct nvlist *tmp;
	int ret;

	/* skip the really boring tags */
	if (count <= 1)
		return;

	tmp = nvl_alloc();
	if (!tmp)
		return;

	if ((ret = nvl_set_str(tmp, "name", str_getref(name))))
		goto err;
	if ((ret = nvl_set_int(tmp, "count", count)))
		goto err;
	if ((ret = nvl_set_int(tmp, "size", __tag_size(count, cmin, cmax))))
		goto err;

	state->cloud[state->ntags].type = NVT_NVL;
	state->cloud[state->ntags].nvl = tmp;
	state->ntags++;

	return;

err:
	/*
	 * On error, we simply skip over this tag.  There is no reason to
	 * fail since the tag cloud isn't actually a vital part of the whole
	 * system.
	 */
	nvl_putref(tmp);
}

static void tagcloud(struct req *req)
{
	struct tagcloud_state state;

	/* gather up tag info */
	index_for_each_tag(__tagcloud_init, __tagcloud_step, &state);

	/* stash the info in request vars */
	vars_set_array(&req->vars, "tagcloud", state.cloud, state.ntags);
}

void sidebar(struct req *req)
{
	tagcloud(req);
}
