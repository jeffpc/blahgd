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

int safe_setxattr(char *path, char *name, void *value, size_t len)
{
	char xpath[FILENAME_MAX];
	int fd;
	int ret;

	snprintf(xpath, FILENAME_MAX, "%s/xattr.%s", path, name);

	fd = open(xpath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd<0)
		return errno;

	ret = write(fd, value, len);
	if (ret<0)
		goto fail;

	close(fd);
	return 0;

fail:
	close(fd);
	return errno;
}

char *safe_getxattr(char *path, char *attr_name)
{
	char xpath[FILENAME_MAX];
	char tmp[4096];
	int ret;
	int fd;

	snprintf(xpath, FILENAME_MAX, "%s/xattr.%s", path, attr_name);

	fd = open(xpath, O_RDONLY);
	if (fd<0)
		return NULL;

	ret = read(fd, tmp, sizeof(tmp)-1);
	if (ret<0)
		goto fail;

	tmp[(tmp[ret-1] == '\n') ? (ret-1) : ret] = '\0';

	close(fd);
	return strdup(tmp);

fail:
	close(fd);
	return NULL;
}
