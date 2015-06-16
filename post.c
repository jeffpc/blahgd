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
#include "db.h"
#include "parse.h"
#include "error.h"
#include "utils.h"

static umem_cache_t *post_cache;
static umem_cache_t *comment_cache;

void init_post_subsys(void)
{
	post_cache = umem_cache_create("post-cache", sizeof(struct post),
				       0, NULL, NULL, NULL, NULL, NULL, 0);
	ASSERT(post_cache);

	comment_cache = umem_cache_create("comment-cache",
					  sizeof(struct comment), 0, NULL,
					  NULL, NULL, NULL, NULL, 0);
	ASSERT(comment_cache);
}

static char *load_comment(struct post *post, int commid)
{
	char *err_msg;
	char path[FILENAME_MAX];
	char *out;

	err_msg = xstrdup("Error: could not load comment text.");

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/comments/%d/text.txt",
		 post->id, commid);

	out = read_file(path);
	if (!out)
		out = err_msg;
	else
		free(err_msg);

	return out;
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
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	ASSERT3U(post->fmt, >=, 1);
	ASSERT3U(post->fmt, <=, 3);

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/post.%s", post->id,
		 exts[post->fmt]);

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return errno;

	ret = fstat(fd, &statbuf);
	if (ret == -1)
		goto err_close;

	ibuf = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (ibuf == MAP_FAILED) {
		ret = errno;
		goto err_close;
	}

	if (post->fmt == 3)
		ret = __do_load_post_body_fmt3(post, ibuf, statbuf.st_size);
	else
		ret = __do_load_post_body(post, ibuf, statbuf.st_size);

	munmap(ibuf, statbuf.st_size);

err_close:
	close(fd);

	return ret;
}

static int __load_post_comments(struct post *post)
{
	sqlite3_stmt *stmt;
	int ret;

	SQL(stmt, "SELECT id, author, email, strftime(\"%s\", time), "
	    "remote_addr, url FROM comments WHERE post=? AND moderated=1 "
	    "ORDER BY id");
	SQL_BIND_INT(stmt, 1, post->id);
	SQL_FOR_EACH(stmt) {
		struct comment *comm;

		comm = umem_cache_alloc(comment_cache, 0);
		ASSERT(comm);

		list_insert_tail(&post->comments, comm);

		comm->id     = SQL_COL_INT(stmt, 0);
		comm->author = xstrdup_def(SQL_COL_STR(stmt, 1), "[unknown]");
		comm->email  = xstrdup(SQL_COL_STR(stmt, 2));
		comm->time   = SQL_COL_INT(stmt, 3);
		comm->ip     = xstrdup(SQL_COL_STR(stmt, 4));
		comm->url    = xstrdup(SQL_COL_STR(stmt, 5));
		comm->body   = load_comment(post, comm->id);

		post->numcom++;
	}

	SQL_END(stmt);

	return 0;
}

struct post *load_post(int postid, bool preview)
{
	char path[FILENAME_MAX];
	int ret;
	int err;
	struct post *post;
	sqlite3_stmt *stmt;

	post = umem_cache_alloc(post_cache, 0);
	if (!post)
		return NULL;

	memset(post, 0, sizeof(struct post));

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d", postid);

	post->id = postid;
	post->title = NULL;
	post->body = NULL;
	post->numcom = 0;
	list_create(&post->tags, sizeof(struct post_tag),
		    offsetof(struct post_tag, list));
	list_create(&post->comments, sizeof(struct comment),
		    offsetof(struct comment, list));

	if (preview) {
		post->title = xstrdup("PREVIEW");
		post->time  = time(NULL);
		post->fmt   = 3;

		err = 0;
	} else {
		SQL(stmt, "SELECT title, strftime(\"%s\", time), fmt FROM posts WHERE id=?");
		SQL_BIND_INT(stmt, 1, postid);
		SQL_FOR_EACH(stmt) {
			post->title = xstrdup(SQL_COL_STR(stmt, 0));
			post->time  = SQL_COL_INT(stmt, 1);
			post->fmt   = SQL_COL_INT(stmt, 2);
		}

		SQL_END(stmt);

		if (!post->title) {
			err = ENOENT;
			goto err;
		}

		SQL(stmt, "SELECT tag FROM post_tags WHERE post=? ORDER BY tag");
		SQL_BIND_INT(stmt, 1, postid);
		SQL_FOR_EACH(stmt) {
			struct post_tag *tag;

			tag = malloc(sizeof(struct post_tag));
			ASSERT(tag);

			tag->tag = xstrdup(SQL_COL_STR(stmt, 0));
			ASSERT(tag->tag);

			list_insert_tail(&post->tags, tag);
		}

		SQL_END(stmt);

		if ((err = __load_post_comments(post)))
			goto err;
	}

	if ((err = __load_post_body(post)))
		goto err;

	return post;

err:
	destroy_post(post);
	return NULL;
}

void destroy_post(struct post *post)
{
	struct post_tag *pt;
	struct comment *com;

	while ((pt = list_remove_head(&post->tags))) {
		free(pt->tag);
		free(pt);
	}

	while ((com = list_remove_head(&post->comments))) {
		free(com->author);
		free(com->email);
		free(com->ip);
		free(com->url);
		free(com->body);
		umem_cache_free(comment_cache, com);
	}

	free(post->title);
	str_putref(post->body);

	umem_cache_free(post_cache, post);
}
