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

#include <jeffpc/str.h>
#include <jeffpc/list.h>

#include "iter.h"
#include "post.h"
#include "req.h"

static int __tag_val(struct nvlist *post, avl_tree_t *list)
{
	struct post_tag *cur;
	struct nvval *tags;
	size_t ntags;
	size_t i;
	int ret;

	ntags = avl_numnodes(list);

#ifdef HAVE_REALLOCARRAY
	tags = reallocarray(NULL, ntags, sizeof(struct nvval));
#else
	tags = malloc(ntags * sizeof(struct nvval));
#endif
	if (!tags)
		return -ENOMEM;

	i = 0;
	avl_for_each(list, cur) {
		struct nvval *tag = &tags[i++];

		tag->type = NVT_STR;
		tag->str = str_getref(cur->tag);
	}

	ret = nvl_set_array(post, "tags", tags, ntags);
	if (ret) {
		nvval_release_array(tags, ntags);
		free(tags);
	}

	return ret;
}

static int __com_val(struct nvlist *post, struct list *list)
{
	struct comment *cur;
	struct nvval *comments;
	size_t ncomments;
	int ret;
	int i;

	/* count the comments */
	ncomments = 0;
	list_for_each(cur, list)
		ncomments++;

	/* no comments = nothing to set in the nvlist */
	if (!ncomments)
		return 0;

#ifdef HAVE_REALLOCARRAY
	comments = reallocarray(NULL, ncomments, sizeof(struct nvval));
#else
	comments = malloc(ncomments * sizeof(struct nvval));
#endif
	if (!comments)
		return -ENOMEM;

	i = 0;
	list_for_each(cur, list) {
		struct nvval *comment = &comments[i++];
		struct nvlist *c;

		c = nvl_alloc();
		if (!c) {
			ret = -ENOMEM;
			goto err;
		}

		comment->type = NVT_NVL;
		comment->nvl = c;

		if ((ret = nvl_set_int(c, "commid", cur->id)))
			goto err;
		if ((ret = nvl_set_int(c, "commtime", cur->time)))
			goto err;
		if ((ret = nvl_set_str(c, "commauthor", str_getref(cur->author))))
			goto err;
		if ((ret = nvl_set_str(c, "commemail", str_getref(cur->email))))
			goto err;
		if ((ret = nvl_set_str(c, "commip", str_getref(cur->ip))))
			goto err;
		if ((ret = nvl_set_str(c, "commurl", str_getref(cur->url))))
			goto err;
		if ((ret = nvl_set_str(c, "commbody", str_getref(cur->body))))
			goto err;
	}

	ret = nvl_set_array(post, "comments", comments, ncomments);
	if (ret)
		goto err;

	return 0;

err:
	nvval_release_array(comments, i);
	free(comments);

	return ret;
}

static struct nvlist *__store_vars(struct req *req, struct post *post,
				   const char *titlevar)
{
	struct nvlist *out;
	int ret;

	if (titlevar) {
		vars_set_str(&req->vars, titlevar, str_getref(post->title));
		vars_set_str(&req->vars, "twittertitle",
			     str_getref(post->title));

		/*
		 * Only set the twitter image if we're dealing with an
		 * individual post - this happens when the titlevar is not
		 * NULL.
		 */
		if (post->twitter_img) {
			struct str *tmp;

			tmp = str_cat(3, str_getref(config.photo_base_url),
				      STATIC_STR("/"),
				      str_getref(post->twitter_img));

			vars_set_str(&req->vars, "twitterimg", tmp);
		}
	}

	out = nvl_alloc();
	if (!out) {
		ret = -ENOMEM;
		goto err;
	}

	if ((ret = nvl_set_int(out, "id", post->id)))
		goto err_nvl;
	if ((ret = nvl_set_int(out, "time", post->time)))
		goto err_nvl;
	if ((ret = nvl_set_str(out, "title", str_getref(post->title))))
		goto err_nvl;
	if ((ret = nvl_set_int(out, "numcom", post->numcom)))
		goto err_nvl;
	if ((ret = nvl_set_str(out, "body", str_getref(post->body))))
		goto err_nvl;

	if ((ret = __tag_val(out, &post->tags)))
		goto err_nvl;
	if ((ret = __com_val(out, &post->comments)))
		goto err_nvl;

	return out;

err_nvl:
	nvl_putref(out);

err:
	return ERR_PTR(ret);
}

struct nvlist *get_post(struct req *req, int postid, const char *titlevar,
			bool preview)
{
	struct nvlist *out;
	struct post *post;

	post = load_post(postid, preview);
	if (!post)
		return NULL;

	post_lock(post, true);

	out = __store_vars(req, post, titlevar);

	post_unlock(post);
	post_putref(post);

	return out;
}

/*
 * Fill in the `posts' array with all posts matching the prepared and bound
 * statement.
 *
 * `stmt' should be all ready to execute and it should output two columns:
 *     post id
 *     post time
 */
void load_posts(struct req *req, struct post **posts, int nposts,
		bool moreposts)
{
	struct nvval *nvposts;
	size_t nnvposts;
	time_t maxtime;
	size_t i;

	maxtime = 0;

#ifdef HAVE_REALLOCARRAY
	nvposts = reallocarray(NULL, nposts, sizeof(struct nvval));
#else
	nvposts = malloc(nposts * sizeof(struct nvval));
#endif
	ASSERT(nvposts);

	nnvposts = 0;

	for (i = 0; i < nposts; i++) {
		struct post *post = posts[i];

		post_lock(post, true);

		nvposts[nnvposts].type = NVT_NVL;
		nvposts[nnvposts].nvl = __store_vars(req, post, NULL);
		if (IS_ERR(nvposts[nnvposts].nvl)) {
			post_unlock(post);
			post_putref(post);
			continue;
		}

		if (post->time > maxtime)
			maxtime = post->time;

		post_unlock(post);
		post_putref(post);

		nnvposts++;
	}

	vars_set_array(&req->vars, "posts", nvposts, nnvposts);
	vars_set_int(&req->vars, "lastupdate", maxtime);
	vars_set_int(&req->vars, "moreposts", moreposts);
}
