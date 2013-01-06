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
#include <assert.h>

#include "post.h"
#include "list.h"
#include "avl.h"
#include "vars.h"
#include "main.h"
#include "db.h"

#if 0
#define CATP_SKIP	0
#define CATP_ECHO	1
#define CATP_PAR	2

static void __do_cat_post(struct post_old *post, char *ibuf, int len)
{
	int sidx, eidx;
	int state = CATP_SKIP;
	char tmp;

	for(eidx = sidx = 0; eidx < len; eidx++) {
		tmp = ibuf[eidx];
#if 0
		printf("\n|'%c' %d| ",tmp, state);
#endif

		switch(state) {
			case CATP_SKIP:
				if (tmp != '\n') {
					fwrite(ibuf+sidx, 1, eidx-sidx, post->out);
					if (post->fmt != 2)
						fwrite("<p>", 1, 3, post->out);
					sidx = eidx;
					state = CATP_ECHO;
				}
				break;

			case CATP_ECHO:
				if (tmp == '\n')
					state = CATP_PAR;
				break;

			case CATP_PAR:
				fwrite(ibuf+sidx, 1, eidx-sidx, post->out);
				sidx = eidx;
				if (tmp == '\n') {
					if (post->fmt != 2)
						fwrite("</p>\n", 1, 5, post->out);
					state = CATP_SKIP;
				} else if (post->fmt == 1) {
					state = CATP_ECHO;
				} else {
					if (post->fmt != 2)
						fwrite("<br/>\n", 1, 5, post->out);
					state = CATP_ECHO;
				}
				break;

		}
	}

	if (state != CATP_SKIP) {
		fwrite(ibuf+sidx, 1, eidx-sidx, post->out);
		if (post->fmt != 2)
			fwrite("</p>", 1, 4, post->out);
	}
}

void cat_post_comment(struct post_old *post, struct comment *comm)
{
	char path[FILENAME_MAX];
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	snprintf(path, FILENAME_MAX, "%s/comments/%d/text.txt", post->path,
		 comm->id);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		fprintf(post->out, "post.txt open error\n");
		return;
	}

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		fprintf(post->out, "fstat failed\n");
		goto out_close;
	}

	ibuf = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (ibuf == MAP_FAILED) {
		fprintf(post->out, "mmap failed\n");
		goto out_close;
	}

	__do_cat_post(post, ibuf, statbuf.st_size);

	munmap(ibuf, statbuf.st_size);

out_close:
	close(fd);
}

int load_comment(struct post_old *post, int commid, struct comment *comm)
{
	char path[FILENAME_MAX];
	char *buf;

	snprintf(path, FILENAME_MAX, "%s/comments/%d", post->path, commid);

	comm->id = commid;
	comm->author = safe_getxattr(path, XATTR_COMM_AUTHOR);
	buf          = safe_getxattr(path, XATTR_TIME);

	if (comm->author && buf && (strlen(buf) == 16)) {
		/* "2005-01-02 03:03" */
		strptime(buf, "%Y-%m-%d %H:%M", &comm->time);

		free(buf);

		return 0;
	}

	free(buf);
	return ENOMEM;
}

void destroy_comment(struct comment *comm)
{
	free(comm->author);
}
#endif

static int __do_load_post_body_fmt3(struct post *post, const char *path)
{
	post->body = strdup("!!!FMT3!!!");
	assert(post->body);

	return 0;
}

static int __do_load_post_body(struct post *post, char *ibuf, size_t len)
{
	post->body = strdup("!!!OLDFMT!!!");
	assert(post->body);

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

	snprintf(path, FILENAME_MAX, "data/posts/%d/post.%s", post->id,
		 exts[post->fmt]);

	if (post->fmt == 3)
		return __do_load_post_body_fmt3(post, path);

	fd = open(path, O_RDONLY);
	assert(fd != -1);

	ret = fstat(fd, &statbuf);
	assert(ret != -1);

	ibuf = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	assert(ibuf != MAP_FAILED);

	__do_load_post_body(post, ibuf, statbuf.st_size);

	munmap(ibuf, statbuf.st_size);

	close(fd);

	return 0;
}

static struct var *__int_var(const char *name, uint64_t val)
{
	struct var *v;

	v = var_alloc(name);
	assert(v);

	v->val[0].type = VT_INT;
	v->val[0].i    = val;

	return v;
}

static struct var *__str_var(const char *name, const char *val)
{
	struct var *v;

	v = var_alloc(name);
	assert(v);

	v->val[0].type = VT_STR;
	v->val[0].str  = val ? strdup(val) : NULL;
	assert(!val || v->val[0].str);

	return v;
}

static struct var *__tag_var(const char *name, struct list_head *val)
{
	struct post_tag *cur, *tmp;
	struct var *v;
	int i;

	v = var_alloc(name);
	assert(v);

	i = 0;
	list_for_each_entry_safe(cur, tmp, val, list) {
		assert(i < VAR_MAX_ARRAY_SIZE);

		v->val[i].type = VT_STR;
		v->val[i].str  = strdup(cur->tag);
		assert(v->val[i].str);

		i++;
	}

	return v;
}

static void __store_vars(struct req *req, const char *var, struct post *post)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type    = VT_VARS;
	vv.vars[0] = __int_var("id", post->id);
	vv.vars[1] = __int_var("time", post->time);
	vv.vars[2] = __str_var("title", post->title);
	vv.vars[3] = __tag_var("tags", &post->tags);
	vv.vars[4] = __str_var("body", post->body);
	vv.vars[5] = __int_var("numcom", 0);

	assert(!var_append(&req->vars, "posts", &vv));
}

int load_post(struct req *req, int postid)
{
	char path[FILENAME_MAX];
	struct post post;
	int ret;
	int err;
	sqlite3_stmt *stmt;

	snprintf(path, FILENAME_MAX, "data/posts/%d", postid);

	post.id = postid;
	post.body = NULL;
	INIT_LIST_HEAD(&post.tags);

	open_db();
	SQL(stmt, "SELECT title, time, fmt FROM posts WHERE id=?");
	SQL_BIND_INT(stmt, 1, postid);
	SQL_FOR_EACH(stmt) {
		post.title = strdup(SQL_COL_STR(stmt, 0));
		post.time  = SQL_COL_INT(stmt, 1);
		post.fmt   = SQL_COL_INT(stmt, 2);
	}

	err = 0;
	SQL(stmt, "SELECT tag FROM post_tags WHERE post=? ORDER BY tag");
	SQL_BIND_INT(stmt, 1, postid);
	SQL_FOR_EACH(stmt) {
		struct post_tag *tag;

		tag = malloc(sizeof(struct post_tag));
		assert(tag);

		tag->tag = strdup(SQL_COL_STR(stmt, 0));
		assert(tag->tag);

		list_add_tail(&tag->list, &post.tags);
	}

	if (!err)
		err = __load_post_body(&post);

	if (err)
		destroy_post(&post);
	else
		__store_vars(req, "posts", &post);

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
