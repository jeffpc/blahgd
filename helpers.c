#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "helpers.h"
#include "scgi.h"
#include "mx.h"

#define NHELPERS	32

static int nhelpers;
static pthread_t helpers[NHELPERS];

static pthread_mutex_t lock;
static pthread_cond_t enqueued;
static LIST_INIT(queue);

static void xsend(int fd, void *buf, size_t len)
{
	ssize_t ret;

	ret = send(fd, buf, len, 0);
	ASSERT3S(ret, !=, -1);
	ASSERT3S(ret, ==, len);
}

static void process_request(struct conn *conn)
{
	char buf[200];
	size_t len;

	len = snprintf(buf, sizeof(buf), "Status: 200 OK\r\n");
	xsend(conn->fd, buf, len);

	len = snprintf(buf, sizeof(buf), "Content-Type: text/plain\r\n");
	xsend(conn->fd, buf, len);

	len = snprintf(buf, sizeof(buf), "\r\n");
	xsend(conn->fd, buf, len);

	len = snprintf(buf, sizeof(buf), "the content!");
	xsend(conn->fd, buf, len);
}

static void *queue_processor(void *arg)
{
	struct conn *conn;

	for (;;) {
		MXLOCK(&lock);
		while (list_empty(&queue))
			CONDWAIT(&enqueued, &lock);

		conn = list_first_entry(&queue, struct conn, queue);

		list_del(&conn->queue);
		MXUNLOCK(&lock);

		// FIXME: actually service the conn
		scgi_read_request(conn);

		process_request(conn);

		close(conn->fd);
		nvlist_free(conn->headers);
		free(conn->body);
		free(conn);
	}

	return NULL;
}

int enqueue_fd(int fd)
{
	struct conn *conn;

	conn = malloc(sizeof(struct conn));
	if (!conn)
		return ENOMEM;

	conn->fd = fd;
	conn->body = NULL;

	MXLOCK(&lock);
	list_add_tail(&conn->queue, &queue);
	CONDSIG(&enqueued);
	MXUNLOCK(&lock);

	return 0;
}

int start_helpers()
{
	int ret;
	int i;

	MXINIT(&lock);
	CONDINIT(&enqueued);

	for (i = 0; i < NHELPERS; i++) {
		ret = pthread_create(&helpers[i], NULL, queue_processor, NULL);
		if (ret) {
			stop_helpers();
			return ret;
		}
	}

	return 0;
}

void stop_helpers()
{
	int i;

	for (i = 0; i < nhelpers; i++)
		pthread_join(helpers[i], NULL);

	nhelpers = 0;

	CONDDESTROY(&enqueued);
	MXDESTROY(&lock);
}
