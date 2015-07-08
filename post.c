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

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <umem.h>

#include "iter.h"
#include "post.h"
#include "vars.h"
#include "req.h"
#include "parse.h"
#include "error.h"
#include "utils.h"
#include "file_cache.h"

static umem_cache_t *post_cache;
static umem_cache_t *comment_cache;

static avl_tree_t posts;
static pthread_mutex_t posts_lock;

static int post_cmp(const void *va, const void *vb)
{
	const struct post *a = va;
	const struct post *b = vb;

	if (a->id < b->id)
		return -1;
	if (a->id > b->id)
		return 1;
	return 0;
}

void init_post_subsys(void)
{
	post_cache = umem_cache_create("post-cache", sizeof(struct post),
				       0, NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(post_cache);

	comment_cache = umem_cache_create("comment-cache",
					  sizeof(struct comment), 0, NULL,
					  NULL, NULL, NULL, NULL, 0);
	ASSERT(comment_cache);

	MXINIT(&posts_lock);

	avl_create(&posts, post_cmp, sizeof(struct post),
		   offsetof(struct post, cache));
}

static struct post *lookup_post(int postid)
{
	struct post *post;
	struct post key = {
		.id = postid,
	};

	MXLOCK(&posts_lock);
	post = avl_find(&posts, &key, NULL);
	post_getref(post);
	MXUNLOCK(&posts_lock);

	return post;
}

static void insert_post(struct post *post)
{
	struct post *tmp;
	avl_index_t where;

	post_getref(post);

	MXLOCK(&posts_lock);
	tmp = avl_find(&posts, post, &where);
	if (!tmp)
		avl_insert(&posts, post, where);
	MXUNLOCK(&posts_lock);

	/* someone beat us to inserting the post, just release what we got */
	if (tmp)
		post_putref(post);
}

void revalidate_post(void *arg)
{
	struct post *post = arg;

	printf("%s: marking post #%u for refresh\n", __func__, post->id);

	post_lock(post, false);
	post->needs_refresh = true;
	post_unlock(post);
}

void revalidate_all_posts(void *arg)
{
	struct post *post;

	printf("%s: marking all posts for refresh\n", __func__);

	MXLOCK(&posts_lock);
	avl_for_each(&posts, post) {
		post_lock(post, false);
		post->needs_refresh = true;
		post_unlock(post);
	}
	MXUNLOCK(&posts_lock);
}

static void post_remove_all_tags(struct post *post)
{
	struct post_tag *tag;

	while ((tag = list_remove_head(&post->tags))) {
		free(tag->tag);
		free(tag);
	}
}

static void post_add_tag(struct post *post, const char *name)
{
	struct post_tag *tag;

	tag = malloc(sizeof(struct post_tag));
	ASSERT(tag);

	tag->tag = xstrdup(name);
	ASSERT(tag->tag);

	list_insert_tail(&post->tags, tag);
}

static void post_remove_all_comments(struct post *post)
{
	struct comment *com;

	while ((com = list_remove_head(&post->comments))) {
		free(com->author);
		free(com->email);
		free(com->ip);
		free(com->url);
		str_putref(com->body);
		umem_cache_free(comment_cache, com);
	}

	post->numcom = 0;
}

static struct str *load_comment(struct post *post, int commid)
{
	char path[FILENAME_MAX];
	struct str *err_msg;
	struct str *out;

	err_msg = STR_DUP("Error: could not load comment text.");

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/comments/%d/text.txt",
		 post->id, commid);

	out = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				post);
	if (!out)
		out = err_msg;
	else
		str_putref(err_msg);

	return out;
}

static void post_add_comment(struct post *post, int commid)
{
	static const struct convert_info table[] = {
		{ .name = "moderated",	.type = DATA_TYPE_BOOLEAN_VALUE, },
		{ .name = NULL, },
	};

	char path[FILENAME_MAX];
	struct comment *comm;
	struct str *meta;
	nvlist_t *nvl;

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/comments/%d/meta.yml",
		 post->id, commid);

	meta = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				 post);
	ASSERT(meta);

	nvl = nvl_from_yaml(meta->str, str_len(meta));
	fprintf(stderr, "yaml comm nvl = %p\n", nvl);
	nvl_convert(nvl, table);

	if (!nvl_lookup_bool(nvl, "moderated"))
		goto done;

	comm = umem_cache_alloc(comment_cache, 0);
	ASSERT(comm);

	comm->id     = commid;
	comm->author = xstrdup(nvl_lookup_str(nvl, "author")); // XXX: default: "[unknown]"
	comm->email  = xstrdup(nvl_lookup_str(nvl, "email"));
	comm->time   = parse_time(nvl_lookup_str(nvl, "time"));
	comm->ip     = xstrdup(nvl_lookup_str(nvl, "remoteaddr"));
	comm->url    = xstrdup(nvl_lookup_str(nvl, "url"));
	comm->body   = load_comment(post, comm->id);

	list_insert_tail(&post->comments, comm);

	post->numcom++;

done:
	nvlist_free(nvl);
	str_putref(meta);
}

static void post_add_comment_str(struct post *post, const char *idstr)
{
	uint32_t i;
	int ret;

	ret = str2u32(idstr, &i);
	ASSERT0(ret);

	post_add_comment(post, i);
}

static int __do_load_post_body_fmt3(struct post *post, char *ibuf, size_t len)
{
	struct parser_output x;
	int ret;

	x.req    = NULL;
	x.post   = post;
	x.input  = ibuf;
	x.len    = strlen(ibuf);
	x.pos    = 0;
	x.lineno = 0;

	post->table_nesting = 0;
	post->texttt_nesting = 0;

	fmt3_lex_init(&x.scanner);
	fmt3_set_extra(&x, x.scanner);

	ret = fmt3_parse(&x);
	if (ret) {
		LOG("failed to parse post id %u", post->id);
		ASSERT(0);
	}

	fmt3_lex_destroy(x.scanner);

	str_putref(post->body); /* free the previous */
	post->body = x.stroutput;
	ASSERT(post->body);

	return 0;
}

/*
 * Concatenate @a with @b.  Only @blen chars are appended to the output.
 *
 * Note: @a will be freed
 */
static char *cc(char *a, char *b, int blen)
{
	int alen;
	char *ret;

	alen = strlen(a);

	ret = malloc(alen + blen + 1);
	ASSERT(ret);

	memcpy(ret, a, alen);
	memcpy(ret + alen, b, blen);
	ret[alen + blen] = '\0';

	free(a);

	return ret;
}

#define CATP_SKIP	0
#define CATP_ECHO	1
#define CATP_PAR	2

static int __do_load_post_body(struct post *post, char *ibuf, size_t len)
{
	int sidx, eidx;
	int state = CATP_SKIP;
	char *ret;
	char tmp;

	ret = xstrdup("");
	ASSERT(ret);

	for(eidx = sidx = 0; eidx < len; eidx++) {
		tmp = ibuf[eidx];
#if 0
		printf("\n|'%c' %d| ",tmp, state);
#endif

		switch(state) {
			case CATP_SKIP:
				if (tmp != '\n') {
					ret = cc(ret, ibuf + sidx, eidx - sidx);
					if (post->fmt != 2)
						ret = cc(ret, "<p>", 3);
					sidx = eidx;
					state = CATP_ECHO;
				}
				break;

			case CATP_ECHO:
				if (tmp == '\n')
					state = CATP_PAR;
				break;

			case CATP_PAR:
				ret = cc(ret, ibuf + sidx, eidx - sidx);
				sidx = eidx;
				if (tmp == '\n') {
					if (post->fmt != 2)
						ret = cc(ret, "</p>\n", 5);
					state = CATP_SKIP;
				} else if (post->fmt == 1) {
					state = CATP_ECHO;
				} else {
					if (post->fmt != 2)
						ret = cc(ret, "<br/>\n", 6);
					state = CATP_ECHO;
				}
				break;

		}
	}

	if (state != CATP_SKIP) {
		ret = cc(ret, ibuf + sidx, eidx - sidx);
		if (post->fmt != 2)
			ret = cc(ret, "</p>", 4);
	}

	str_putref(post->body); /* free the previous */
	post->body = str_alloc(ret);
	ASSERT(post->body);

	return 0;
}

static int __load_post_body(struct post *post)
{
	static const char *exts[4] = {
		[1] = "txt",
		[2] = "txt",
		[3] = "tex",
	};

	char path[FILENAME_MAX];
	struct str *raw;
	int ret;

	ASSERT3U(post->fmt, >=, 1);
	ASSERT3U(post->fmt, <=, 3);

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/post.%s", post->id,
		 exts[post->fmt]);

	raw = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				post);
	if (!raw)
		return ENOENT;

	if (post->fmt == 3)
		ret = __do_load_post_body_fmt3(post, raw->str, str_len(raw));
	else
		ret = __do_load_post_body(post, raw->str, str_len(raw));

	str_putref(raw);

	return ret;
}

static void __refresh_published_prop(struct post *post, nvlist_t *nvl)
{
	const char *sval;
	uint64_t ival;

	/* update the time */
	sval = nvl_lookup_str(nvl, "time");
	post->time = parse_time(sval);

	/* update the title */
	sval = nvl_lookup_str(nvl, "title");
	post->title = xstrdup(sval);

	/* update the format */
	ival = nvl_lookup_int(nvl, "fmt");
	post->fmt = ival;
}

static int __refresh_published(struct post *post)
{
	static const struct convert_info table[] = {
		{ .name = "fmt",	.type = DATA_TYPE_UINT64, },
		{ .name = NULL, },
	};

	char path[FILENAME_MAX];
	struct str *meta;
	nvlist_t *nvl;

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/post.yml", post->id);

	meta = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
				 post);
	if (!meta)
		return ENOENT;

	nvl = nvl_from_yaml(meta->str, str_len(meta));
	fprintf(stderr, "yaml nvl = %p\n", nvl);
	nvl_convert(nvl, table);

	__refresh_published_prop(post, nvl);

	// XXX: post_add_tag(post, X);
	// XXX: post_add_cat(post, X);
	// XXX: post_add_comment_str(post, X);

	nvlist_free(nvl);
	str_putref(meta);

	return 0;
}

int __refresh(struct post *post)
{
	int ret;

	printf("refreshing post...\n");

	free(post->title);
	post->title = NULL;

	if (post->preview) {
		post->title = xstrdup("PREVIEW");
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
		post = lookup_post(postid);
		if (post)
			return post;
	}

	post = umem_cache_alloc(post_cache, 0);
	if (!post)
		return NULL;

	memset(post, 0, sizeof(struct post));

	post->id = postid;
	post->title = NULL;
	post->body = NULL;
	post->numcom = 0;
	post->preview = preview;
	post->needs_refresh = false;

	list_create(&post->tags, sizeof(struct post_tag),
		    offsetof(struct post_tag, list));
	list_create(&post->comments, sizeof(struct comment),
		    offsetof(struct comment, list));
	refcnt_init(&post->refcnt, 1);
	MXINIT(&post->lock);

	if ((err = __refresh(post)))
		goto err;

	if (!post->preview)
		insert_post(post);

	return post;

err:
	post_destroy(post);
	return NULL;
}

void post_destroy(struct post *post)
{
	post_remove_all_tags(post);
	post_remove_all_comments(post);

	free(post->title);
	str_putref(post->body);

	MXDESTROY(&post->lock);

	umem_cache_free(post_cache, post);
}
