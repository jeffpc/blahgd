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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <string.h>

#include <jeffpc/error.h>

#include "req.h"
#include "vars.h"
#include "utils.h"

static int read_netstring_length(struct req *req, size_t *len)
{
	ssize_t ret;
	size_t v;

	v = 0;

	for (;;) {
		char c;

		ret = recv(req->fd, &c, sizeof(c), 0);
		ASSERT3S(ret, !=, -1);
		ASSERT3S(ret, ==, 1);

		switch (c) {
			case ':':
				*len = v;
				return 0;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9':
				v = (v * 10) + (c - '0');
				break;
			default:
				return -EINVAL;
		}
	}
}

static int read_netstring_string(struct req *req, size_t len)
{
	struct nvlist *nvl;
	char *cur, *end;
	char *buf;
	int ret;

	buf = malloc(len + 1);
	if (!buf)
		return -ENOMEM;

	nvl = nvl_alloc();
	if (!nvl) {
		ret = -ENOMEM;
		goto err;
	}

	ret = xread(req->fd, buf, len + 1);
	if (ret)
		goto err;

	ASSERT3U(buf[len], ==, ',');

	buf[len] = '\0';

	/*
	 * Now, parse the received string.
	 */

	cur = buf;
	end = buf + len;

	while (cur < end) {
		char *name, *val;

		name = cur;
		val = cur + strlen(name) + 1;

		nvl_set_str(nvl, name, STR_DUP(val));

		cur = val + strlen(val) + 1;
	}

	req->request_headers = nvl;

	free(buf);

	return 0;

err:
	free(buf);
	return ret;
}

static void cvt_headers(struct req *req)
{
	static const struct nvl_convert_info table[] = {
		{ .name = "SCGI",		.tgt_type = NVT_INT, },
		{ .name = NULL, },
	};

	convert_headers(req->request_headers, table);
}

static int read_netstring(struct req *req)
{
	size_t len;
	int ret;

	ret = read_netstring_length(req, &len);
	if (ret)
		return ret;

	ret = read_netstring_string(req, len);
	if (ret)
		return ret;

	cvt_headers(req);

	return 0;
}

static int read_body(struct req *req)
{
	uint64_t content_len;
	ssize_t ret;
	char *buf;

	ret = nvl_lookup_int(req->request_headers, CONTENT_LENGTH,
			     &content_len);
	if (ret)
		return ret;

	if (!content_len)
		return 0;

	buf = malloc(content_len + 1);
	if (!buf)
		return -ENOMEM;

	ret = recv(req->fd, buf, content_len, MSG_WAITALL);
	ASSERT3S(ret, !=, -1);
	ASSERT3S(ret, ==, content_len);

	buf[content_len] = '\0';

	req->request_body = buf;

	return 0;
}

int scgi_read_request(struct req *req)
{
	int ret;

	/* read the headers */
	ret = read_netstring(req);
	if (ret)
		return ret;

	/* read the body */
	return read_body(req);
}
