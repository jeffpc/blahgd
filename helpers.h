#ifndef __HELPERS_H
#define __HELPERS_H

#include <libnvpair.h>

#include "list.h"

struct conn {
	struct list_head queue;
	int fd;

	nvlist_t *headers;
	char *body;
};

extern int start_helpers();
extern void stop_helpers();
extern int enqueue_fd(int fd);

#endif
