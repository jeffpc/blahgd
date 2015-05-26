#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "error.h"
#include "utils.h"
#include "helpers.h"
#include "atomic.h"
#include "db.h"
#include "val.h"
#include "pipeline.h"
#include "template_cache.h"
#include "math.h"

#define HOST		NULL
#define PORT		2014
#define CONN_BACKLOG	32

union sockaddr_union {
	struct sockaddr_in inet;
	struct sockaddr_in6 inet6;
};

#define MAX_SOCK_FDS		8
static int fds[MAX_SOCK_FDS];
static int nfds;

atomic_t server_shutdown;

static void sigterm_handler(int signum, siginfo_t *info, void *unused)
{
	LOG("SIGTERM received");

	atomic_set(&server_shutdown, 1);
}

static void handle_signals(void)
{
	struct sigaction action;
	int ret;

	sigemptyset(&action.sa_mask);

	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;

	ret = sigaction(SIGPIPE, &action, NULL);
	if (ret)
		LOG("Failed to ignore SIGPIPE: %s", strerror(errno));

	action.sa_sigaction = sigterm_handler;
	action.sa_flags = SA_SIGINFO;

	ret = sigaction(SIGTERM, &action, NULL);
	if (ret)
		LOG("Failed to set SIGTERM handler: %s", strerror(errno));
}

static int bind_sock(int family, struct sockaddr *addr, int addrlen)
{
	int on = 1;
	int ret;
	int fd;

	if (nfds >= MAX_SOCK_FDS)
		return EMFILE;

	fd = socket(family, SOCK_STREAM, 0);
	if (fd == -1)
		return errno;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(fd, addr, addrlen))
		goto err;

	if (listen(fd, CONN_BACKLOG))
		goto err;

	fds[nfds++] = fd;

	return 0;

err:
	ret = errno;

	close(fd);

	return ret;
}

static int start_listening(void)
{
	struct addrinfo hints, *res, *p;
	char port[10];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	snprintf(port, sizeof(port), "%d", PORT);

	if (getaddrinfo(HOST, port, &hints, &res))
		return -1;

	for (p = res; p; p = p->ai_next) {
		struct sockaddr_in *ipv4;
		struct sockaddr_in6 *ipv6;
		char str[64];
		void *addr;
		int ret;

		switch (p->ai_family) {
			case AF_INET:
				ipv4 = (struct sockaddr_in *) p->ai_addr;
				addr = &ipv4->sin_addr;
				break;
			case AF_INET6:
				ipv6 = (struct sockaddr_in6 *) p->ai_addr;
				addr = &ipv6->sin6_addr;
				break;
			default:
				LOG("Unsupparted address family: %d",
				    p->ai_family);
				addr = NULL;
				break;
		}

		if (!addr)
			continue;

		inet_ntop(p->ai_family, addr, str, sizeof(str));

		ret = bind_sock(p->ai_family, p->ai_addr, p->ai_addrlen);
		if (ret && ret != EAFNOSUPPORT)
			return ret;
		else if (!ret)
			LOG("Bound to: %s %s", str, port);
	}

	freeaddrinfo(res);

	return 0;
}

static void stop_listening(void)
{
	int i;

	for (i = 0; i < nfds; i++)
		close(fds[i]);
}

static void accept_conns(void)
{
	fd_set set;
	int maxfd;
	int ret;
	int i;

	for (;;) {
		union sockaddr_union addr;
		unsigned len;
		int fd;

		FD_ZERO(&set);
		maxfd = 0;
		for (i = 0; i < nfds; i++) {
			FD_SET(fds[i], &set);
			maxfd = MAX(maxfd, fds[i]);
		}

		ret = select(maxfd + 1, &set, NULL, NULL, NULL);

		if (atomic_read(&server_shutdown))
			break;

		if ((ret < 0) && (errno != EINTR))
			LOG("Error on select: %s", strerror(errno));

		for (i = 0; (i < nfds) && (ret > 0); i++) {
			if (!FD_ISSET(fds[i], &set))
				continue;

			len = sizeof(addr);
			fd = accept(fds[i], (struct sockaddr *) &addr, &len);
			if (fd == -1) {
				LOG("Failed to accept from fd %d: %s",
				    fds[i], strerror(errno));
				continue;
			}

			if (enqueue_fd(fd))
				close(fd);

			ret--;
		}
	}
}

int main(int argc, char **argv)
{
	int ret;

	init_math(true);
	init_val_subsys();
	init_pipe_subsys();
	init_template_cache();

	init_db();

	handle_signals();

	ret = start_helpers();
	if (ret)
		goto err;

	ret = start_listening();
	if (ret)
		goto err;

	accept_conns();

	stop_listening();

	stop_helpers();

	return 0;

err:
	stop_helpers();

	LOG("Failed to inintialize: %s", strerror(ret));

	return ret;
}
