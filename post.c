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
#include "avl.h"
#include "db.h"

#if 0
void cat(struct post_old *post, void *data, char *tmpl, char *fmt,
	 struct repltab_entry *repltab)
{
	char path[FILENAME_MAX];
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	snprintf(path, sizeof(path), "templates/%s.%s", tmpl, fmt);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		fprintf(post->out, "template (%s) open error\n", path);
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

	sar(post, data, ibuf, statbuf.st_size, repltab);

	munmap(ibuf, statbuf.st_size);

out_close:
	close(fd);
}

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

void cat_post(struct post_old *post)
{
	char *exts[4] = {
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

	if (post->preview)
		fprintf(post->out, "<p><strong>NOTE: This is only a preview."
			" This post hasn't been published yet. The post's"
			" title, categories, and publication time will"
			" change.</strong></p>\n");

	snprintf(path, FILENAME_MAX, "%s/post.%s", post->path, exts[post->fmt]);

	if (post->fmt == 3) {
		__do_cat_post_fmt3(post, path);
		return;
	}

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

static void __each_comment_helper(struct post_old *post, char *name, void *data)
{
	void(*f)(struct post_old*, struct comment*) = data;
	struct comment comm;
	int commid;

	commid = atoi(name);

	if (load_comment(post, commid, &comm))
		return;

	f(post, &comm);

	destroy_comment(&comm);
}

void invoke_for_each_comment(struct post_old *post, void(*f)(struct post_old*,
							 struct comment*))
{
	char path[FILENAME_MAX];
	DIR *dir;

	snprintf(path, FILENAME_MAX, "%s/comments", post->path);

	dir = opendir(path);
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __each_comment_helper, f, SORT_ASC,
			    0, -1);

	closedir(dir);
}
#endif

int load_post(int postid, struct post *post)
{
	char path[FILENAME_MAX];
	int ret = 0;
	sqlite3_stmt *stmt;

	snprintf(path, FILENAME_MAX, "data/posts/%d", postid);

	post->id = postid;

	open_db();
	SQL(stmt, "SELECT title, time, fmt FROM posts WHERE id=?");
	SQL_BIND_INT(stmt, 1, postid);
	SQL_FOR_EACH(stmt) {
		post->title = strdup(SQL_COL_STR(stmt, 0));
		post->time  = SQL_COL_INT(stmt, 1);
		post->fmt   = SQL_COL_INT(stmt, 2);
	}

	post->tags = NULL;
	SQL(stmt, "SELECT tag FROM post_tags WHERE post=? ORDER BY tag");
	SQL_BIND_INT(stmt, 1, postid);
	SQL_FOR_EACH(stmt) {
		const char *tag = strdup(SQL_COL_STR(stmt, 0));

		if (!post->tags) {
			post->tags = strdup(tag);
		} else {
			char *buf2;
			int len;

			len  = strlen(post->tags) + 1 + strlen(tag) + 1;
			buf2 = malloc(len);
			if (!buf2) {
				ret = ENOMEM;
				break;
			}

			snprintf(buf2, len, "%s,%s", post->tags, tag);
			free(post->tags);
			post->tags = buf2;
		}
	}

	if (ret)
		destroy_post(post);

	return ret;
}

void destroy_post(struct post *post)
{
	free(post->title);
	free(post->tags);
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
