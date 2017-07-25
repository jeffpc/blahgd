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

#include <priv.h>

#include <jeffpc/jeffpc.h>
#include <jeffpc/version.h>
#include <jeffpc/error.h>
#include <jeffpc/atomic.h>
#include <jeffpc/val.h>
#include <jeffpc/str.h>
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

static void process_request(struct scgi *scgi, void *private)
{
	struct req *req;
	uint64_t now;

	now = gettime();

	req = req_alloc();
	if (!req)
		return;

	req_init(req);

	req->stats.dequeue = now;
	req->scgi = scgi;

	req_dispatch(req);

	req_output(req);
	req_destroy(req);
	req_free(req);
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

int main(int argc, char **argv)
{
	int ret;

	ASSERT0(putenv("TZ=UTC"));

	cmn_err(CE_INFO, "blahgd version %s", version_string);
	cmn_err(CE_INFO, "libjeffpc version %s", jeffpc_version);

	/* drop unneeded privs */
	ret = drop_privs();
	if (ret)
		goto err;

	jeffpc_init(&init_ops);
	init_math(true);
	init_pipe_subsys();
	init_req_subsys();
	init_post_subsys();
	init_file_cache();

	ret = config_load((argc >= 2) ? argv[1] : NULL);
	if (ret)
		goto err;

	ret = init_wordpress_categories();
	if (ret)
		goto err;

	ret = load_all_posts();
	if (ret)
		goto err;

	ret = scgisvc(NULL, config.scgi_port, config.scgi_threads,
		      process_request, NULL);
	if (ret)
		goto err;

	free_all_posts();
	uncache_all_files();

	return 0;

err:
	DBG("Failed to inintialize: %s", xstrerror(ret));

	return ret;
}
