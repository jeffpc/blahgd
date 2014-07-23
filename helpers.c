#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "iter.h"
#include "req.h"
#include "qstring.h"
#include "helpers.h"
#include "scgi.h"
#include "mx.h"

static int nhelpers;
static pthread_t helpers[SCGI_NHELPERS];

static pthread_mutex_t lock;
static pthread_cond_t enqueued;
static list_t queue;

static void process_request(struct req *req)
{
	parse_query_string(req->request_qs,
			   nvl_lookup_str(req->request_headers,
					  "QUERY_STRING"));

	req_dispatch(req);

	req_output(req);
	req_destroy(req);
}

static void *queue_processor(void *arg)
{
	struct req *req;

	for (;;) {
		MXLOCK(&lock);
		while (list_is_empty(&queue))
			CONDWAIT(&enqueued, &lock);

		req = list_remove_head(&queue);
		MXUNLOCK(&lock);

		scgi_read_request(req);

		process_request(req);

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
	list_insert_tail(&queue, req);
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
	list_create(&queue, sizeof(struct req), offsetof(struct req,
							 scgi.queue));

	for (i = 0; i < SCGI_NHELPERS; i++) {
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
