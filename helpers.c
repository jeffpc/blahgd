#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "req.h"
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

static void process_request(struct req *req)
{
	char buf[200];
	size_t len;

	len = snprintf(buf, sizeof(buf), "Status: 200 OK\r\n");
	xsend(req->scgi.fd, buf, len);

	len = snprintf(buf, sizeof(buf), "Content-Type: text/plain\r\n");
	xsend(req->scgi.fd, buf, len);

	len = snprintf(buf, sizeof(buf), "\r\n");
	xsend(req->scgi.fd, buf, len);

	len = snprintf(buf, sizeof(buf), "the content!");
	xsend(req->scgi.fd, buf, len);
}

static void *queue_processor(void *arg)
{
	struct req *req;

	for (;;) {
		MXLOCK(&lock);
		while (list_empty(&queue))
			CONDWAIT(&enqueued, &lock);

		req = list_first_entry(&queue, struct req, scgi.queue);

		list_del(&req->scgi.queue);
		MXUNLOCK(&lock);

		scgi_read_request(req);

		nvl_dump(req->request_headers);

		process_request(req);

		req_destroy(req);
		free(req);
	}

	return NULL;
}

int enqueue_fd(int fd)
{
	struct req *req;

	req = malloc(sizeof(struct req));
	if (!req)
		return ENOMEM;

	req_init_scgi(req, fd);

	MXLOCK(&lock);
	list_add_tail(&req->scgi.queue, &queue);
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