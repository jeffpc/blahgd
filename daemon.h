#ifndef __DAEMON_H
#define __DAEMON_H

struct conn {
	pthread_t self;
	int fd;
};

#endif
