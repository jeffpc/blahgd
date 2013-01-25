#include <stdlib.h>
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
#include "avl.h"
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

	snprintf(path, FILENAME_MAX, "data/posts/%d/comments/%d/text.txt", post->id,
		 commid);

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

	x.req   = NULL;
	x.post  = post;
	x.input = ibuf;
	x.len   = strlen(ibuf);
	x.pos   = 0;

	fmt3_lex_init(&x.scanner);
	fmt3_set_extra(&x, x.scanner);

	ASSERT(fmt3_parse(&x) == 0);

	fmt3_lex_destroy(x.scanner);

	post->body = x.output;
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

	post->body = ret;
	ASSERT(post->body);

	return 0;
}

static int __load_post_body(struct post *post)
{
	static const char *exts[4] = {
		[0] = "txt",
		[1] = "txt",
		[2] = "txt",
		[3] = "tex",
	};

	char path[FILENAME_MAX];
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	ASSERT3U(post->fmt, <=, 3);

	snprintf(path, FILENAME_MAX, "data/posts/%d/post.%s", post->id,
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

	post->numcom = 0;

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

static struct var *__int_var(const char *name, uint64_t val)
{
	struct var *v;

	v = var_alloc(name);
	ASSERT(v);

	v->val[0].type = VT_INT;
	v->val[0].i    = val;

	return v;
}

static struct var *__str_var(const char *name, const char *val)
{
	struct var *v;

	v = var_alloc(name);
	ASSERT(v);

	v->val[0].type = VT_STR;
	v->val[0].str  = xstrdup(val);
	ASSERT(!val || v->val[0].str);

	return v;
}

static struct var *__tag_var(const char *name, struct list_head *val)
{
	struct post_tag *cur, *tmp;
	struct var *v;
	int i;

	v = var_alloc(name);
	ASSERT(v);

	i = 0;
	list_for_each_entry_safe(cur, tmp, val, list) {
		ASSERT(i < VAR_MAX_ARRAY_SIZE);

		v->val[i].type = VT_STR;
		v->val[i].str  = xstrdup(cur->tag);
		ASSERT(v->val[i].str);

		i++;
	}

	return v;
}

static struct var *__com_var(const char *name, struct list_head *val)
{
	struct comment *cur, *tmp;
	struct var *v;
	int i;

	v = var_alloc(name);
	ASSERT(v);

	i = 0;
	list_for_each_entry_safe(cur, tmp, val, list) {
		ASSERT(i < VAR_MAX_ARRAY_SIZE);

		v->val[i].type = VT_VARS;
		v->val[i].vars[0] = __int_var("commid", cur->id);
		v->val[i].vars[1] = __int_var("commtime", cur->time);
		v->val[i].vars[2] = __str_var("commauthor", cur->author);
		v->val[i].vars[3] = __str_var("commemail", cur->email);
		v->val[i].vars[4] = __str_var("commip", cur->ip);
		v->val[i].vars[5] = __str_var("commurl", cur->url);
		v->val[i].vars[6] = __str_var("commbody", cur->body);

		i++;
	}

	return v;
}

static void __store_vars(struct req *req, const char *var, struct post *post,
			 const char *titlevar)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type    = VT_VARS;
	vv.vars[0] = __int_var("id", post->id);
	vv.vars[1] = __int_var("time", post->time);
	vv.vars[2] = __str_var("title", post->title);
	vv.vars[3] = __tag_var("tags", &post->tags);
	vv.vars[4] = __str_var("body", post->body);
	vv.vars[5] = __int_var("numcom", post->numcom);
	vv.vars[6] = __com_var("comments", &post->comments);

	ASSERT(!var_append(&req->vars, "posts", &vv));

	if (titlevar) {
		vv.type = VT_STR;
		vv.str  = xstrdup(post->title);
		ASSERT(vv.str);

		ASSERT(!var_append(&req->vars, titlevar, &vv));
	}
}

int load_post(struct req *req, int postid, const char *titlevar)
{
	char path[FILENAME_MAX];
	struct post post;
	int ret;
	int err;
	sqlite3_stmt *stmt;

	snprintf(path, FILENAME_MAX, "data/posts/%d", postid);

	post.id = postid;
	post.title = NULL;
	post.body = NULL;
	post.numcom = 0;
	INIT_LIST_HEAD(&post.tags);
	INIT_LIST_HEAD(&post.comments);

	open_db();
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

	if (!err)
		err = __load_post_body(&post);

err:
	if (err)
		destroy_post(&post);
	else
		__store_vars(req, "posts", &post, titlevar);

	return err;
}

void destroy_post(struct post *post)
{
	free(post->title);
}

#if 0
void dump_post(struct post_old *post)
{
	if (!post)
		fprintf(stdout, "p=NULL\n");
	else
		fprintf(post->out, "p=%p { %d, '%s', '%s', '%s', '%04d-%02d-%02d %02d:%02d' }\n\n",
			post, post->id, post->title, post->cats,
			post->tags,
			post->time.tm_year, post->time.tm_mon,
			post->time.tm_mday, post->time.tm_hour,
			post->time.tm_min);
}
#endif
