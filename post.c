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

#include "post.h"
#include "list.h"
#include "vars.h"
#include "main.h"
#include "db.h"
#include "parse.h"
#include "error.h"
#include "utils.h"

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

	post->body = str_to_val(x.stroutput);
	ASSERT(post->body);

	return 0;
}

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

	post->body = VAL_ALLOC_STR(ret);
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

		comm = malloc(sizeof(struct comment));
		ASSERT(comm);

		list_add_tail(&comm->list, &post->comments);

		comm->id     = SQL_COL_INT(stmt, 0);
		comm->author = xstrdup_def(SQL_COL_STR(stmt, 1), "[unknown]");
		comm->email  = xstrdup(SQL_COL_STR(stmt, 2));
		comm->time   = SQL_COL_INT(stmt, 3);
		comm->ip     = xstrdup(SQL_COL_STR(stmt, 4));
		comm->url    = xstrdup(SQL_COL_STR(stmt, 5));
		comm->body   = load_comment(post, comm->id);

		post->numcom++;
	}

	return 0;
}

static struct val *__tag_val(struct list_head *list)
{
	struct post_tag *cur, *tmp;
	struct val *val;
	struct val *sub;
	int i;

	val = VAL_ALLOC(VT_LIST);

	i = 0;
	list_for_each_entry_safe(cur, tmp, list, list) {
		sub = VAL_ALLOC_STR(xstrdup(cur->tag));

		VAL_SET_LIST(val, i, sub);
		i++;
	}

	return val;
}

static struct val *__com_val(struct list_head *list)
{
	struct comment *cur, *tmp;
	struct val *val;
	struct val *sub;
	int i;

	val = VAL_ALLOC(VT_LIST);

	i = 0;
	list_for_each_entry_safe(cur, tmp, list, list) {
		sub = VAL_ALLOC(VT_NV);

		VAL_SET_NVINT(sub, "commid", cur->id);
		VAL_SET_NVINT(sub, "commtime", cur->time);
		VAL_SET_NVSTR(sub, "commauthor", xstrdup(cur->author));
		VAL_SET_NVSTR(sub, "commemail", xstrdup(cur->email));
		VAL_SET_NVSTR(sub, "commip", xstrdup(cur->ip));
		VAL_SET_NVSTR(sub, "commurl", xstrdup(cur->url));
		VAL_SET_NVSTR(sub, "commbody", xstrdup(cur->body));

		VAL_SET_LIST(val, i, sub);
		i++;
	}

	return val;
}

static struct val *__store_vars(struct req *req, struct post *post, const char *titlevar)
{
	struct val *val;

	if (titlevar) {
		val = VAL_ALLOC_STR(xstrdup(post->title));
		VAR_SET(&req->vars, titlevar, val);
	}

	val = VAL_ALLOC(VT_NV);

	VAL_SET_NVINT(val, "id", post->id);
	VAL_SET_NVINT(val, "time", post->time);
	VAL_SET_NVSTR(val, "title", xstrdup(post->title));
	VAL_SET_NV   (val, "tags", __tag_val(&post->tags));
	VAL_SET_NV   (val, "body", val_getref(post->body));
	VAL_SET_NVINT(val, "numcom", post->numcom);
	VAL_SET_NV   (val, "comments", __com_val(&post->comments));

	return val;
}

struct val *load_post(struct req *req, int postid, const char *titlevar, bool preview)
{
	char path[FILENAME_MAX];
	struct post post;
	int ret;
	int err;
	sqlite3_stmt *stmt;
	struct val *val;

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d", postid);

	post.id = postid;
	post.title = NULL;
	post.body = NULL;
	post.numcom = 0;
	INIT_LIST_HEAD(&post.tags);
	INIT_LIST_HEAD(&post.comments);

	if (preview) {
		post.title = xstrdup("PREVIEW");
		post.time  = time(NULL);
		post.fmt   = 3;

		err = 0;
	} else {
		SQL(stmt, "SELECT title, strftime(\"%s\", time), fmt FROM posts WHERE id=?");
		SQL_BIND_INT(stmt, 1, postid);
		SQL_FOR_EACH(stmt) {
			post.title = xstrdup(SQL_COL_STR(stmt, 0));
			post.time  = SQL_COL_INT(stmt, 1);
			post.fmt   = SQL_COL_INT(stmt, 2);
		}

		if (!post.title) {
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

			list_add_tail(&tag->list, &post.tags);
		}

		err = __load_post_comments(&post);
	}

	if (!err)
		err = __load_post_body(&post);

err:
	if (!err)
		val = __store_vars(req, &post, titlevar);

	destroy_post(&post);

	return err ? NULL : val;
}

void destroy_post(struct post *post)
{
	struct post_tag *pt, *pttmp;
	struct comment *com, *comtmp;

	list_for_each_entry_safe(pt, pttmp, &post->tags, list) {
		free(pt->tag);
		free(pt);
	}

	list_for_each_entry_safe(com, comtmp, &post->comments, list) {
		free(com->author);
		free(com->email);
		free(com->ip);
		free(com->url);
		free(com->body);
		free(com);
	}

	free(post->title);
	val_putref(post->body);
}

/*
 * Fill in the `posts' array with all posts matching the prepared and bound
 * statement.
 *
 * `stmt' should be all ready to execute and it should output two columns:
 *     post id
 *     post time
 */
void load_posts(struct req *req, sqlite3_stmt *stmt)
{
	struct val *posts;
	struct val *val;
	time_t maxtime;
	int ret;
	int i;

	maxtime = 0;
	i = 0;

	posts = VAR_LOOKUP_VAL(&req->vars, "posts");

	SQL_FOR_EACH(stmt) {
		time_t posttime;
		int postid;

		postid   = SQL_COL_INT(stmt, 0);
		posttime = SQL_COL_INT(stmt, 1);

		val = load_post(req, postid, NULL, false);
		if (!val)
			continue;

		VAL_SET_LIST(posts, i++, val);

		if (posttime > maxtime)
			maxtime = posttime;
	}

	val_putref(posts);

	VAR_SET_INT(&req->vars, "lastupdate", maxtime);
}
