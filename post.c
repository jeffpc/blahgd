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

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include <jeffpc/taskq.h>
#include <jeffpc/error.h>
#include <jeffpc/io.h>
#include <jeffpc/mem.h>

#include "iter.h"
#include "post.h"
#include "vars.h"
#include "req.h"
#include "parse.h"
#include "utils.h"
#include "file_cache.h"
#include "debug.h"

static struct mem_cache *post_cache;
static struct mem_cache *comment_cache;

static void post_remove_all_tags(avl_tree_t *taglist);
static void post_remove_all_comments(struct post *post);

static int tag_cmp(const void *va, const void *vb)
{
	const struct post_tag *a = va;
	const struct post_tag *b = vb;
	int ret;

	ret = strcasecmp(str_cstr(a->tag), str_cstr(b->tag));

	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

void init_post_subsys(void)
{
	post_cache = mem_cache_create("post-cache", sizeof(struct post), 0);
	ASSERT(!IS_ERR(post_cache));

	comment_cache = mem_cache_create("comment-cache",
					 sizeof(struct comment), 0);
	ASSERT(!IS_ERR(comment_cache));

	init_post_index();
}

void revalidate_post(void *arg)
{
	struct post *post = arg;

	DBG("%s: marking post #%u for refresh", __func__, post->id);

	post_lock(post, false);
	post->needs_refresh = true;
	post_unlock(post);
}

static void post_add_tags(avl_tree_t *taglist, struct val *list)
{
	struct post_tag *tag;

	/* tags list not present in metadata */
	if (!list)
		return;

	for (; list; list = sexpr_cdr(list)) {
		struct val *tagval;
		struct str *tagname;

		tagval = sexpr_car(val_getref(list));

		/* sanity check */
		ASSERT3U(tagval->type, ==, VT_STR);

		/* get the tag name */
		tagname = str_getref(tagval->str);

		/* release the tag value */
		val_putref(tagval);

		tag = malloc(sizeof(struct post_tag));
		ASSERT(tag);

		tag->tag = tagname;
		ASSERT(tag->tag);

		if (safe_avl_add(taglist, tag)) {
			/* found a duplicate */
			str_putref(tag->tag);
			free(tag);
		}
	}
}

static void post_remove_all_comments(struct post *post)
{
	struct comment *com;

	while ((com = list_remove_head(&post->comments))) {
		str_putref(com->author);
		str_putref(com->email);
		str_putref(com->ip);
		str_putref(com->url);
		str_putref(com->body);
		mem_cache_free(comment_cache, com);
	}

	post->numcom = 0;
}

static struct str *load_comment(struct post *post, int commid)
{
	char path[FILENAME_MAX];
	struct str *err_msg;
	struct str *out;

	err_msg = STATIC_STR("Error: could not load comment text.");

	snprintf(path, FILENAME_MAX, "%s/posts/%d/comments/%d/text.txt",
		 str_cstr(config.data_dir), post->id, commid);

	out = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				post);
	if (IS_ERR(out))
		out = err_msg;
	else
		str_putref(err_msg);

	return out;
}

static void post_add_comment(struct post *post, int commid)
{
	char path[FILENAME_MAX];
	struct comment *comm;
	struct str *meta;
	struct val *lv;
	struct val *v;

	snprintf(path, FILENAME_MAX, "%s/posts/%d/comments/%d/meta.lisp",
		 str_cstr(config.data_dir), post->id, commid);

	meta = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				 post);
	ASSERT(!IS_ERR(meta));

	lv = sexpr_parse_str(meta);
	ASSERT(!IS_ERR(lv));

	v = sexpr_cdr(sexpr_assoc(lv, "moderated"));
	if (!v || (v->type != VT_BOOL) || !v->b)
		goto done;

	comm = mem_cache_alloc(comment_cache);
	ASSERT(comm);

	comm->id     = commid;
	comm->author = sexpr_alist_lookup_str(lv, "author");
	comm->email  = sexpr_alist_lookup_str(lv, "email");
	comm->time   = parse_time_str(sexpr_alist_lookup_str(lv, "time"));
	comm->ip     = sexpr_alist_lookup_str(lv, "ip");
	comm->url    = sexpr_alist_lookup_str(lv, "url");
	comm->body   = load_comment(post, comm->id);

	if (!comm->author)
		comm->author = STATIC_STR("[unknown]");

	list_insert_tail(&post->comments, comm);

	post->numcom++;

done:
	val_putref(v);
	val_putref(lv);
	str_putref(meta);
}

static void post_add_comments(struct post *post, struct val *list)
{
	if (!list)
		return;

	for (; list; list = sexpr_cdr(list)) {
		struct val *val;

		val = sexpr_car(val_getref(list));

		/* sanity check */
		ASSERT3U(val->type, ==, VT_INT);

		/* add the comment */
		post_add_comment(post, val->i);

		/* release the value */
		val_putref(val);
	}
}

static int __do_load_post_body_fmt3(struct post *post, const struct str *input)
{
	struct parser_output x;
	int ret;

	x.req            = NULL;
	x.post           = post;
	x.input          = str_cstr(input);
	x.len            = str_len(input);
	x.pos            = 0;
	x.lineno         = 0;
	x.table_nesting  = 0;
	x.texttt_nesting = 0;
	x.sc_title       = NULL;
	x.sc_pub         = NULL;
	x.sc_tags        = NULL;
	x.sc_cats        = NULL;
	x.sc_twitter_img = NULL;

	fmt3_lex_init(&x.scanner);
	fmt3_set_extra(&x, x.scanner);

	ret = fmt3_parse(&x);
	if (ret)
		panic("failed to parse post id %u", post->id);

	fmt3_lex_destroy(x.scanner);

	/*
	 * Now update struct post based on what we got from the .tex file.
	 * The struct is already populated by data from the metadata file.
	 * For the simple string values, we merely override whatever was
	 * there.  For tags and categories we use the union.
	 */

	if (x.sc_title) {
		str_putref(post->title);
		post->title = str_getref(x.sc_title);
	}

	if (x.sc_pub)
		post->time = parse_time_str(str_getref(x.sc_pub));

	if (x.sc_twitter_img) {
		str_putref(post->twitter_img);
		post->twitter_img = str_getref(x.sc_twitter_img);
	}

	post_add_tags(&post->tags, x.sc_tags);
	post_add_tags(&post->cats, x.sc_cats);

	str_putref(x.sc_title);
	str_putref(x.sc_pub);
	str_putref(x.sc_twitter_img);

	str_putref(post->body); /* free the previous */
	post->body = x.stroutput;
	ASSERT(post->body);

	return 0;
}

static int __load_post_body(struct post *post)
{
	static const char *exts[4] = {
		[3] = "tex",
	};

	char path[FILENAME_MAX];
	struct str *raw;
	int ret;

	ASSERT3U(post->fmt, ==, 3);

	snprintf(path, FILENAME_MAX, "%s/posts/%d/post.%s",
		 str_cstr(config.data_dir), post->id, exts[post->fmt]);

	raw = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				post);
	if (IS_ERR(raw))
		return PTR_ERR(raw);

	ret = __do_load_post_body_fmt3(post, raw);

	str_putref(raw);

	return ret;
}

static void __refresh_published_prop(struct post *post, struct val *lv)
{
	/* update the time */
	post->time = parse_time_str(sexpr_alist_lookup_str(lv, "time"));

	/* update the title */
	post->title = sexpr_alist_lookup_str(lv, "title");

	/* update the format */
	post->fmt = sexpr_alist_lookup_int(lv, "fmt", NULL);

	/* update the listed bool */
	post->listed = sexpr_alist_lookup_bool(lv, "listed", true, NULL);
}

static int __refresh_published(struct post *post)
{
	char path[FILENAME_MAX];
	struct str *meta;
	struct val *lv;

	snprintf(path, FILENAME_MAX, "%s/posts/%d/post.lisp",
		 str_cstr(config.data_dir), post->id);

	meta = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				 post);
	if (IS_ERR(meta))
		return PTR_ERR(meta);

	lv = sexpr_parse_str(meta);
	if (IS_ERR(lv)) {
		str_putref(meta);
		return PTR_ERR(lv);
	}

	__refresh_published_prop(post, lv);

	/* empty out the tags/cats/comments lists */
	post_remove_all_tags(&post->tags);
	post_remove_all_tags(&post->cats);
	post_remove_all_comments(post);

	/* populate the tags/cats/comments lists */
	post_add_tags(&post->tags, sexpr_alist_lookup_list(lv, "tags"));
	post_add_tags(&post->cats, sexpr_alist_lookup_list(lv, "cats"));
	post_add_comments(post, sexpr_alist_lookup_list(lv, "comments"));

	val_putref(lv);
	str_putref(meta);

	return 0;
}

int __refresh(struct post *post)
{
	int ret;

	str_putref(post->title);
	post->title = NULL;

	if (post->preview) {
		post->title = STATIC_STR("PREVIEW");
		post->time  = time(NULL);
		post->fmt   = 3;

		ret = 0;
	} else {
		ret = __refresh_published(post);
		if (ret)
			return ret;
	}

	if ((ret = __load_post_body(post)))
		return ret;

	post->needs_refresh = false;

	return 0;
}

void post_refresh(struct post *post)
{
	ASSERT0(post->preview);
	ASSERT0(__refresh(post));
}

struct post *load_post(int postid, bool preview)
{
	struct post *post;
	int err;

	/*
	 * If it is *not* a preview, try to get it from the cache.
	 */
	if (!preview) {
		post = index_lookup_post(postid);
		if (post)
			return post;
	}

	post = mem_cache_alloc(post_cache);
	if (!post) {
		err = -ENOMEM;
		goto err;
	}

	memset(post, 0, sizeof(struct post));

	post->id = postid;
	post->title = NULL;
	post->body = NULL;
	post->numcom = 0;
	post->preview = preview;
	post->needs_refresh = true;

	avl_create(&post->tags, tag_cmp, sizeof(struct post_tag),
		    offsetof(struct post_tag, node));
	avl_create(&post->cats, tag_cmp, sizeof(struct post_tag),
		    offsetof(struct post_tag, node));
	list_create(&post->comments, sizeof(struct comment),
		    offsetof(struct comment, list));
	refcnt_init(&post->refcnt, 1);
	MXINIT(&post->lock);

	if ((err = __refresh(post)))
		goto err_free;

	if (!post->preview)
		ASSERT0(index_insert_post(post));

	return post;

err_free:
	post_destroy(post);
err:
	cmn_err(CE_ERROR, "Failed to load post id %u: %s", postid,
		xstrerror(err));
	return NULL;
}

static void post_remove_all_tags(avl_tree_t *taglist)
{
	struct post_tag *tag;
	void *cookie;

	cookie = NULL;
	while ((tag = avl_destroy_nodes(taglist, &cookie))) {
		str_putref(tag->tag);
		free(tag);
	}

	avl_create(taglist, tag_cmp, sizeof(struct post_tag),
		    offsetof(struct post_tag, node));
}

void post_destroy(struct post *post)
{
	post_remove_all_tags(&post->tags);
	post_remove_all_tags(&post->cats);
	post_remove_all_comments(post);

	str_putref(post->title);
	str_putref(post->body);

	MXDESTROY(&post->lock);

	mem_cache_free(post_cache, post);
}

static void __tq_load_post(void *arg)
{
	int postid = (uintptr_t) arg;

	/* load the post, but then free it since we don't need it */
	post_putref(load_post(postid, false));
}

int load_all_posts(void)
{
	const char *data_dir = str_cstr(config.data_dir);
	char path[FILENAME_MAX];
	struct stat statbuf;
	struct dirent *de;
	uint32_t postid;
	uint64_t start_ts, end_ts;
	unsigned nposts;
	struct taskq *tq;
	DIR *dir;
	int ret;

	snprintf(path, sizeof(path), "%s/posts", data_dir);
	dir = opendir(path);
	if (!dir)
		return -errno;

	tq = taskq_create_fixed("load-all-posts", -1);
	if (IS_ERR(tq)) {
		closedir(dir);
		return PTR_ERR(tq);
	}

	nposts = 0;
	start_ts = gettime();

	while ((de = readdir(dir))) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		ret = str2u32(de->d_name, &postid);
		if (ret) {
			cmn_err(CE_INFO, "skipping '%s/%s' - not a number",
				data_dir, de->d_name);
			continue;
		}

		snprintf(path, FILENAME_MAX, "%s/posts/%u", data_dir, postid);

		/* check that it is a directory */
		ret = xlstat(path, &statbuf);
		if (ret) {
			cmn_err(CE_INFO, "skipping '%s' - failed to xlstat: %s",
				path, xstrerror(ret));
			continue;
		}

		if (!S_ISDIR(statbuf.st_mode)) {
			cmn_err(CE_INFO, "skipping '%s' - not a directory; "
				"mode = %o", path,
				(unsigned int) statbuf.st_mode);
			continue;
		}

		/* load the post asynchronously */
		if (taskq_dispatch(tq, __tq_load_post, (void *)(uintptr_t) postid))
			__tq_load_post((void *)(uintptr_t) postid);

		nposts++;
	}

	taskq_wait(tq);
	taskq_destroy(tq);

	end_ts = gettime();

	cmn_err(CE_INFO, "Loaded %u posts in %"PRIu64".%09"PRIu64" seconds",
		nposts,
		(end_ts - start_ts) / 1000000000UL,
		(end_ts - start_ts) % 1000000000UL);

	closedir(dir);

	return 0;
}
