#include <stdlib.h>

#include <sys/types.h>
#include <attr/xattr.h>

char *safe_getxattr(char *path, char *attr_name)
{
	ssize_t size;
	char *buf;

	size = getxattr(path, attr_name, NULL, 0);
	if (size == -1)
		return NULL;

	buf = malloc(size+1);
	if (!buf)
		return NULL;

	size = getxattr(path, attr_name, buf, size+1);
	if (size == -1) {
		free(buf);
		return NULL;
	}

	return buf;
}
