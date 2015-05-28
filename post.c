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

#include "iter.h"
#include "post.h"
#include "vars.h"
#include "req.h"
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

		comm = malloc(sizeof(struct comment));
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

static void __tag_val(nvlist_t *post, list_t *list)
{
	struct post_tag *cur;
	char **tags;
	int ntags;
	int i;

	/* count the tags */
	ntags = 0;
	list_for_each(cur, list)
		ntags++;

	tags = malloc(sizeof(char *) * ntags);
	ASSERT(tags);

	i = 0;
	list_for_each(cur, list)
		tags[i++] = cur->tag;

	nvl_set_str_array(post, "tags", tags, ntags);

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
	list_for_each(cur, list)
		ncomments++;

	comments = malloc(sizeof(nvlist_t *) * ncomments);
	ASSERT(comments);

	i = 0;
	list_for_each(cur, list) {
		comments[i] = nvl_alloc();

		nvl_set_int(comments[i], "commid", cur->id);
		nvl_set_int(comments[i], "commtime", cur->time);
		nvl_set_str(comments[i], "commauthor", cur->author);
		nvl_set_str(comments[i], "commemail", cur->email);
		nvl_set_str(comments[i], "commip", cur->ip);
		nvl_set_str(comments[i], "commurl", cur->url);
		nvl_set_str(comments[i], "commbody", cur->body);

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
		vars_set_str(&req->vars, titlevar, post->title);

	out = nvl_alloc();

	nvl_set_int(out, "id", post->id);
	nvl_set_int(out, "time", post->time);
	nvl_set_str(out, "title", post->title);
	nvl_set_int(out, "numcom", post->numcom);
	nvl_set_str(out, "body", post->body->str);

	__tag_val(out, &post->tags);
	__com_val(out, &post->comments);

	return out;
}

nvlist_t *load_post(struct req *req, int postid, const char *titlevar, bool preview)
{
	char path[FILENAME_MAX];
	struct post post;
	int ret;
	int err;
	sqlite3_stmt *stmt;
	nvlist_t *out;

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d", postid);

	post.id = postid;
	post.title = NULL;
	post.body = NULL;
	post.numcom = 0;
	list_create(&post.tags, sizeof(struct post_tag),
		    offsetof(struct post_tag, list));
	list_create(&post.comments, sizeof(struct comment),
		    offsetof(struct comment, list));

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

		SQL_END(stmt);

		SQL(stmt, "SELECT tag FROM post_tags WHERE post=? ORDER BY tag");
		SQL_BIND_INT(stmt, 1, postid);
		SQL_FOR_EACH(stmt) {
			struct post_tag *tag;

			tag = malloc(sizeof(struct post_tag));
			ASSERT(tag);

			tag->tag = xstrdup(SQL_COL_STR(stmt, 0));
			ASSERT(tag->tag);

			list_insert_tail(&post.tags, tag);
		}

		SQL_END(stmt);

		err = __load_post_comments(&post);
	}

	if (!err)
		err = __load_post_body(&post);

err:
	if (!err)
		out = __store_vars(req, &post, titlevar);

	destroy_post(&post);

	return err ? NULL : out;
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
		free(com);
	}

	free(post->title);
	str_putref(post->body);
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

		posts[nposts] = load_post(req, postid, NULL, false);
		if (!posts[nposts])
			continue;

		if (posttime > maxtime)
			maxtime = posttime;

		nposts++;
	}

	vars_set_nvl_array(&req->vars, "posts", posts, nposts);
	vars_set_int(&req->vars, "lastupdate", maxtime);

	for (i = 0; i < nposts; i++)
		nvlist_free(posts[i]);

	free(posts);
}
