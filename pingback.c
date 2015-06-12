/*
 * Copyright (c) 2011-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
#include <stdio.h>
#include <string.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_cgi.h>

#include "post.h"
#include "comment.h"

static xmlrpc_value *ping(xmlrpc_env * const envP,
			  xmlrpc_value * const paramArrayP,
			  void * const serverInfo,
			  void * const channelInfo)
{
	struct post_old post;
	char comment[1024];
	char *src, *tgt;
	char *tmp, *end;
	int ret;
	int id;

	struct timespec now;

	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(ss)", &src, &tgt);
	if (envP->fault_occurred)
		return NULL;

	/* we got a pingback for 'tgt' from 'src' */

	end = tgt + strlen(tgt) - 1;
	tmp = end;
	ret = 1;
	for(;;) {
		if (tmp < src)
			goto out;

		if ((*tmp >= '0') && (*tmp <= '9')) {
			tmp--;
			continue;
		}

		if (tmp == end)
			goto out;

		break;
	}

	id = atoi(tmp+1);

	snprintf(comment, sizeof(comment), "Pingback from <a href=\"%s\">%s</a>\n", src, src);

	clock_gettime(CLOCK_REALTIME, &now);

	post.out = stderr;
	post.id  = 0;
	post.time.tm_year = 0;
	post.time.tm_mon  = 0;
	post.time.tm_mday = 1;

	ret = write_out_comment(&post, id, &now, "Pingback", "", src,
				comment);

	destroy_post(&post);

out:
	free(src);
	free(tgt);

	if (ret)
		return NULL;

	return xmlrpc_build_value(envP, "s", "");
}

int blahg_pingback()
{
	xmlrpc_registry * registryP;
	xmlrpc_env env;

	xmlrpc_env_init(&env);

	registryP = xmlrpc_registry_new(&env);

	xmlrpc_registry_add_method(&env, registryP, NULL, "pingback.ping",
				   &ping, NULL);

	xmlrpc_server_cgi_process_call(registryP);

	return 0;
}
