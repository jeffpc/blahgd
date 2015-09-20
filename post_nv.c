/*
 * Copyright (c) 2009-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include "iter.h"
#include "post.h"
#include "req.h"
#include "str.h"

static void __tag_val(nvlist_t *post, list_t *list)
{
	struct post_tag *cur;
	const char **tags;
	int ntags;
	int i;

	/* count the tags */
	ntags = 0;
	list_for_each(list, cur)
		ntags++;

	tags = malloc(sizeof(char *) * ntags);
	ASSERT(tags);

	i = 0;
	list_for_each(list, cur)
		tags[i++] = str_cstr(cur->tag);

	nvl_set_str_array(post, "tags", (char **) tags, ntags);

	free(tags);
}

static void __com_val(nvlist_t *post, list_t *list)
{
	struct comment *cur;
	nvlist_t **comments;
	uint_t ncomments;
	int i;

	/* count the comments */
	ncomments = 0;
	list_for_each(list, cur)
		ncomments++;

	comments = malloc(sizeof(nvlist_t *) * ncomments);
	ASSERT(comments);

	i = 0;
	list_for_each(list, cur) {
		comments[i] = nvl_alloc();

		nvl_set_int(comments[i], "commid", cur->id);
		nvl_set_int(comments[i], "commtime", cur->time);
		nvl_set_str(comments[i], "commauthor", str_cstr(cur->author));
		nvl_set_str(comments[i], "commemail", str_cstr(cur->email));
		nvl_set_str(comments[i], "commip", str_cstr(cur->ip));
		nvl_set_str(comments[i], "commurl", str_cstr(cur->url));
		nvl_set_str(comments[i], "commbody", str_cstr(cur->body));

		i++;
	}

	nvl_set_nvl_array(post, "comments", comments, ncomments);

	while (--i >= 0)
		nvlist_free(comments[i]);

	free(comments);
}

static nvlist_t *__store_vars(struct req *req, struct post *post, const char *titlevar)
{
	nvlist_t *out;

	if (titlevar)
		vars_set_str(&req->vars, titlevar, str_cstr(post->title));

	out = nvl_alloc();

	nvl_set_int(out, "id", post->id);
	nvl_set_int(out, "time", post->time);
	nvl_set_str(out, "title", str_cstr(post->title));
	nvl_set_int(out, "numcom", post->numcom);
	nvl_set_str(out, "body", str_cstr(post->body));

	__tag_val(out, &post->tags);
	__com_val(out, &post->comments);

	return out;
}

nvlist_t *get_post(struct req *req, int postid, const char *titlevar, bool preview)
{
	struct post *post;
	nvlist_t *out;

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
void load_posts(struct req *req, sqlite3_stmt *stmt, int expected)
{
	nvlist_t **posts;
	uint_t nposts;
	time_t maxtime;
	int ret;
	int i;

	maxtime = 0;

	posts = NULL;
	nposts = 0;

	SQL_FOR_EACH(stmt) {
		time_t posttime;
		int postid;

		postid   = SQL_COL_INT(stmt, 0);
		posttime = SQL_COL_INT(stmt, 1);

		posts = realloc(posts, sizeof(nvlist_t *) * (nposts + 1));
		ASSERT(posts);

		posts[nposts] = get_post(req, postid, NULL, false);
		if (!posts[nposts])
			continue;

		if (posttime > maxtime)
			maxtime = posttime;

		nposts++;
	}

	vars_set_nvl_array(&req->vars, "posts", posts, nposts);
	vars_set_int(&req->vars, "lastupdate", maxtime);
	vars_set_int(&req->vars, "moreposts", nposts == expected);

	for (i = 0; i < nposts; i++)
		nvlist_free(posts[i]);

	free(posts);
}
