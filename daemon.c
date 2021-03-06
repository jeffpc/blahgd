/*
 * Copyright (c) 2014-2020 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include "config.h"

#ifdef HAVE_PRIV_H
#include <priv.h>
#endif

#include <jeffpc/jeffpc.h>
#include <jeffpc/version.h>
#include <jeffpc/error.h>
#include <jeffpc/atomic.h>
#include <jeffpc/val.h>
#include <jeffpc/types.h>
#include <jeffpc/scgisvc.h>
#include <jeffpc/file-cache.h>

#include "utils.h"
#include "pipeline.h"
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
#ifdef HAVE_PRIV_H
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
#else
	cmn_err(CE_WARN, "No supported way to drop privileges");

	return 0;
#endif
}

/* the main daemon process */
static int main_blahgd(int argc, char **argv)
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

	ASSERT0(file_cache_init());

	init_pipe_subsys();
	init_post_subsys();

	ret = load_all_posts();
	if (ret)
		goto err;

	ret = scgisvc(NULL, config.scgi_port, config.scgi_threads,
		      &ops, NULL);
	if (ret)
		goto err;

	free_all_posts();
	file_cache_uncache_all();

	return 0;

err:
	return ret;
}

int main(int argc, char **argv)
{
	int ret;

	ASSERT0(putenv("TZ=UTC"));

	jeffpc_init(&init_ops);

	cmn_err(CE_INFO, "blahgd version %s", version_string);
	cmn_err(CE_INFO, "libjeffpc version %s", jeffpc_version);

	ret = config_load((argc >= 2) ? argv[1] : NULL);
	if (!ret)
		ret = main_blahgd(argc, argv);

	if (ret)
		DBG("Failed to initialize: %s", xstrerror(ret));

	return ret;
}
