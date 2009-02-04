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

#include "post.h"
#include "xattr.h"
#include "sar.h"

void cat(struct post *post, char *tmpl, struct repltab_entry *repltab)
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

	sar(post, ibuf, statbuf.st_size, repltab);

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
	fwrite(ibuf, 1, statbuf.st_size, post->out);

	munmap(ibuf, statbuf.st_size);

out_close:
	close(fd);
}

int load_post(int postid, struct post *post)
{
	char path[FILENAME_MAX];

	snprintf(path, FILENAME_MAX, "data/posts/%d", postid);

	post->id = postid;
	post->title = safe_getxattr(path, XATTR_TITLE);
	post->cats  = safe_getxattr(path, XATTR_CATS);
	post->time  = safe_getxattr(path, XATTR_TIME);

	if (post->title && post->time)
		return 0;

	destroy_post(post);
	return ENOMEM;
}

void destroy_post(struct post *post)
{
	free(post->title);
	free(post->cats);
	free(post->time);
}

void dump_post(struct post *post)
{
	if (!post)
		fprintf(stdout, "p=NULL\n");
	else
		fprintf(post->out, "p=%p { %d, '%s', '%s', '%s' }\n\n",
			post, post->id, post->title, post->cats,
			post->time);
}
