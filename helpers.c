/*
 * Copyright (c) 2014-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <limits.h>

#include <jeffpc/taskq.h>

#include "req.h"
#include "helpers.h"
#include "scgi.h"
#include "utils.h"
#include "debug.h"

static struct taskq *processor;

static void queue_processor(void *arg)
{
	struct req *req = arg;

	req->stats.dequeue = gettime();

	ASSERT0(scgi_read_request(req));

	req_dispatch(req);

	req_output(req);
	req_destroy(req);
	req_free(req);
}

int enqueue_fd(int fd, uint64_t ts)
{
	struct req *req;
	int ret;

	req = req_alloc();
	if (!req)
		return -ENOMEM;

	req->stats.fd_conn = ts;

	req_init_scgi(req, fd);

	ret = taskq_dispatch(processor, queue_processor, req);
	if (ret) {
		DBG("failed to dispatch connection: %s", xstrerror(ret));
		req_destroy(req);
		req_free(req);
	}

	return ret;
}

int start_helpers(void)
{
	processor = taskq_create_fixed("scgi", config.scgi_threads);
	if (IS_ERR(processor))
		return PTR_ERR(processor);

	return 0;
}

void stop_helpers(void)
{
	if (!processor || IS_ERR(processor))
		return;

	taskq_wait(processor);
	taskq_destroy(processor);
}
