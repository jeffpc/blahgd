#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"

#define HDD_START	0
#define HDD_1DOT	1
#define HDD_2DOT	2
#define HDD_OK		3

int hasdotdot(char *path)
{
	int state = HDD_START;
	char tmp;

	while(*path) {
		tmp = *path;

		switch(tmp) {
			case '.':
				if (state == HDD_START)
					state = HDD_1DOT;
				else if (state == HDD_1DOT)
					state = HDD_2DOT;
				else
					state = HDD_OK;
				break;
			case '/':
				if (state == HDD_2DOT)
					return 1; /* '/../' found */

				state = HDD_START;
				break;
			default:
				state = HDD_OK;
				break;
		}

		path++;
	}

	return (state == HDD_2DOT);
}

int xread(int fd, void *buf, size_t nbyte)
{
	char *ptr = buf;
	size_t total;
	int ret;

	total = 0;

	while (nbyte) {
		ret = read(fd, ptr, nbyte);
		if (ret < 0) {
			LOG("%s: failed to read %u bytes from fd %d: %s",
			    __func__, nbyte, fd, strerror(errno));
			return -errno;
		}

		if (ret == 0)
			break;

		nbyte -= ret;
		total += ret;
		ptr   += ret;
	}

	return total;
}

int xwrite(int fd, const void *buf, size_t nbyte)
{
	const char *ptr = buf;
	size_t total;
	int ret;

	total = 0;

	while (nbyte) {
		ret = write(fd, ptr, nbyte);
		if (ret < 0) {
			LOG("%s: failed to write %u bytes to fd %d: %s",
			    __func__, nbyte, fd, strerror(errno));
			return -errno;
		}

		if (ret == 0)
			break;

		nbyte -= ret;
		total += ret;
		ptr   += ret;
	}

	return total;
}

char *read_file(const char *fname)
{
	struct stat statbuf;
	char *out;
	int ret;
	int fd;

	out = NULL;

	fd = open(fname, O_RDONLY);
	if (fd == -1)
		goto err;

	ret = fstat(fd, &statbuf);
	if (ret == -1)
		goto err_close;

	out = malloc(statbuf.st_size + 1);
	if (!out)
		goto err_close;

	ret = xread(fd, out, statbuf.st_size);
	if (ret != statbuf.st_size) {
		free(out);
		out = NULL;
	} else {
		out[statbuf.st_size] = '\0';
	}

err_close:
	close(fd);

err:
	return out;
}

int write_file(const char *fname, const char *data, size_t len)
{
	int ret;
	int fd;

	fd = open(fname, O_WRONLY | O_CREAT | O_EXCL,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1)
		return 1;

	ret = xwrite(fd, data, len);

	close(fd);

	return ret != len;
}

char *concat5(char *a, char *b, char *c, char *d, char *e)
{
	char *src[] = { a, b, c, d, e, };
	char *ret;
	int nsrc;
	int first;
	int tofree;
	int len;
	int i, j;

	nsrc = 0;
	len = 0;
	first = -1;
	tofree = 0x1f;

	for (i = 0; i < 5; i++) {
		if (src[i]) {
			len += strlen(src[i]);
			if (first == -1)
				first = i;

			nsrc++;

			for (j = i + 1; j < 5; j++)
				if (src[i] == src[j])
					tofree &= ~(1 << i);
		}
	}

	if (nsrc == 1)
		return src[first];

	if (!len) {
		for (i = 0; i < 5; i++)
			if (tofree & (1 << i))
				free(src[i]);

		return xstrdup("");
	}

	ret = malloc(len + 1);
	ASSERT(ret);

	for (i = 0; i < 5; i++) {
		if (!src[i])
			continue;

		if (i == first)
			strcpy(ret, src[i]);
		else
			strcat(ret, src[i]);

		if (tofree & (1 << i))
			free(src[i]);
	}

	return ret;
}
