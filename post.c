#define _XOPEN_SOURCE
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
#include "xattr.h"
#include "sar.h"

void cat(struct post *post, void *data, char *tmpl,
	 struct repltab_entry *repltab)
{
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	fd = open(tmpl, O_RDONLY);
	if (fd == -1) {
		fprintf(post->out, "template open error\n");
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

void cat_post(struct post *post)
{
	char path[FILENAME_MAX];
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	snprintf(path, FILENAME_MAX, "data/posts/%d/post.txt", post->id);

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

	/* FIXME: do <p> insertion, etc. */
	fwrite("<p>", 1, 3, post->out);
	fwrite(ibuf, 1, statbuf.st_size, post->out);
	fwrite("</p>", 1, 4, post->out);

	munmap(ibuf, statbuf.st_size);

out_close:
	close(fd);
}

void cat_post_comment(struct post *post, struct comment *comm)
{
	char path[FILENAME_MAX];
	struct stat statbuf;
	char *ibuf;
	int ret;
	int fd;

	snprintf(path, FILENAME_MAX, "data/posts/%d/comments/%d/text.txt",
		 post->id, comm->id);

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

	/* FIXME: do <p> insertion, etc. */
	fwrite("<p>", 1, 3, post->out);
	fwrite(ibuf, 1, statbuf.st_size, post->out);
	fwrite("</p>", 1, 4, post->out);

	munmap(ibuf, statbuf.st_size);

out_close:
	close(fd);
}

int load_comment(struct post *post, int commid, struct comment *comm)
{
	char path[FILENAME_MAX];
	char *buf;

	snprintf(path, FILENAME_MAX, "data/posts/%d/comments/%d", post->id,
		 commid);

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

void invoke_for_each_comment(struct post *post, void(*f)(struct post*,
							 struct comment*))
{
	char path[FILENAME_MAX];
	struct dirent *de;
	DIR *dir;
	struct comment comm;
	int commid;

	snprintf(path, FILENAME_MAX, "data/posts/%d/comments", post->id);

	dir = opendir(path);
	if (!dir)
		return;

	while((de = readdir(dir))) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		commid = atoi(de->d_name);

		if (load_comment(post, commid, &comm))
			break;

		f(post, &comm);

		destroy_comment(&comm);
	}

	closedir(dir);
}

int load_post(int postid, struct post *post)
{
	char path[FILENAME_MAX];
	char *buf;

	snprintf(path, FILENAME_MAX, "data/posts/%d", postid);

	post->id = postid;
	post->title = safe_getxattr(path, XATTR_TITLE);
	post->cats  = safe_getxattr(path, XATTR_CATS);
	buf         = safe_getxattr(path, XATTR_TIME);

	if (post->title && buf && (strlen(buf) == 16)) {
		/* "2005-01-02 03:03" */
		strptime(buf, "%Y-%m-%d %H:%M", &post->time);

		free(buf);

		return 0;
	}

	destroy_post(post);
	free(buf);
	return ENOMEM;
}

void destroy_post(struct post *post)
{
	free(post->title);
	free(post->cats);
}

void dump_post(struct post *post)
{
	if (!post)
		fprintf(stdout, "p=NULL\n");
	else
		fprintf(post->out, "p=%p { %d, '%s', '%s', '%04d-%02d-%02d %02d:%02d' }\n\n",
			post, post->id, post->title, post->cats,
			post->time.tm_year, post->time.tm_mon,
			post->time.tm_mday, post->time.tm_hour,
			post->time.tm_min);
}
