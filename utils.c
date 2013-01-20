#include <unistd.h>
#include <errno.h>

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
