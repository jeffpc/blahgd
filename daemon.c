/*
 * Copyright (c) 2014-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <syslog.h>
#include <priv.h>

#include <jeffpc/jeffpc.h>
#include <jeffpc/version.h>
#include <jeffpc/error.h>
#include <jeffpc/atomic.h>
#include <jeffpc/val.h>
#include <jeffpc/types.h>
#include <jeffpc/scgisvc.h>

#include "utils.h"
#include "pipeline.h"
#include "file_cache.h"
#include "math.h"
#include "req.h"
#include "post.h"
#include "version.h"
#include "debug.h"

static int init_request(struct scgi *scgi, void *private)
{
	struct req *req;

	req = malloc(sizeof(struct req));
	if (!req)
		return -ENOMEM;

	req_init(req, scgi);

	scgi->private = req;

	return 0;
}

static void deinit_request(struct scgi *scgi)
{
	req_destroy(scgi->private);
	free(scgi->private);
}

static void process_request(struct scgi *scgi)
{
	req_dispatch(scgi->private);
	req_output(scgi->private);
}

static int drop_privs()
{
	static const char *privs[] = {
		"file_read",
		"file_write",
		"net_access",
		NULL,
	};
	static const priv_ptype_t privsets[] = {
		PRIV_PERMITTED,
		PRIV_LIMIT,
		PRIV_INHERITABLE,
	};

	priv_set_t *wanted;
	int ret;
	int i;

	wanted = priv_allocset();
	if (!wanted)
		return -errno;

	priv_emptyset(wanted);

	for (i = 0; privs[i]; i++) {
		ret = priv_addset(wanted, privs[i]);
		if (ret) {
			ret = -errno;
			goto err_free;
		}
	}

	for (i = 0; i < ARRAY_LEN(privsets); i++) {
		ret = setppriv(PRIV_SET, privsets[i], wanted);
		if (ret == -1) {
			ret = -errno;
			break;
		}
	}

err_free:
	priv_freeset(wanted);

	return ret;
}

/* the main daemon process */
static int main_blahgd(int argc, char **argv, int mathfd)
{
	static const struct scgiops ops = {
		.init = init_request,
		.process = process_request,
		.deinit = deinit_request,
	};
	int ret;

	/* drop unneeded privs */
	ret = drop_privs();
	if (ret)
		goto err;

	init_math(mathfd);
	init_pipe_subsys();
	init_post_subsys();
	init_file_cache();

	ret = init_wordpress_categories();
	if (ret)
		goto err;

	ret = load_all_posts();
	if (ret)
		goto err;

	ret = scgisvc(NULL, config.scgi_port, config.scgi_threads,
		      &ops, NULL);
	if (ret)
		goto err;

	free_all_posts();
	uncache_all_files();

	return 0;

err:
	return ret;
}

/* the math helper worker */
static int main_mathd(int argc, char **argv, int mathfd)
{
	openlog("mathd", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	return render_math_processor(mathfd);
}

int main(int argc, char **argv)
{
	int mathfds[2];
	pid_t pid;
	int ret;

	ASSERT0(putenv("TZ=UTC"));

	cmn_err(CE_INFO, "blahgd version %s", version_string);
	cmn_err(CE_INFO, "libjeffpc version %s", jeffpc_version);

	jeffpc_init(&init_ops);

	ret = config_load((argc >= 2) ? argv[1] : NULL);
	if (ret)
		goto out;

	ret = pipe(mathfds);
	if (ret == -1) {
		ret = -errno;
		goto out;
	}

	switch ((pid = fork())) {
		case -1:
			/* error */
			ret = -errno;
			break;
		case 0:
			/* child */
			ret = main_mathd(argc, argv, mathfds[0]);
			break;
		default:
			/* parent */
			cmn_err(CE_DEBUG, "math worker pid is %lu",
				(unsigned long) pid);
			ret = main_blahgd(argc, argv, mathfds[1]);
			break;
	}

out:
	if (ret)
		DBG("Failed to initialize: %s", xstrerror(ret));

	return ret;
}
