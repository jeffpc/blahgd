#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>

#include "error.h"
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

		ret = recv(req->scgi.fd, &c, sizeof(c), 0);
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
				return EINVAL;
		}
	}
}

static int read_netstring_string(struct req *req, size_t len)
{
	nvlist_t *nvl;
	char *cur, *end;
	char *buf;
	int ret;

	buf = malloc(len + 1);
	if (!buf)
		return ENOMEM;

	ret = recv(req->scgi.fd, buf, len + 1, MSG_WAITALL);
	ASSERT3S(ret, !=, -1);

	if (ret != (len + 1)) {
		ret = errno;
		free(buf);
		return ret;
	}

	ASSERT3U(buf[len], ==, ',');

	buf[len] = '\0';

	/*
	 * Now, parse the received string.
	 */

	cur = buf;
	end = buf + len;

	nvl = nvl_alloc();

	while (cur < end) {
		char *name, *val;

		name = cur;
		val = cur + strlen(name) + 1;

		nvl_set_str(nvl, name, val);

		cur = val + strlen(val) + 1;
	}

	req->request_headers = nvl;

	free(buf);

	return 0;
}

struct cvth {
	const char *name;
	data_type_t type;
};

static void cvt_headers(struct req *req)
{
	struct cvth table[] = {
		{ .name = "CONTENT_LENGTH", .type = DATA_TYPE_UINT64, },
		{ .name = "REMOTE_PORT", .type = DATA_TYPE_UINT64, },
		{ .name = "SCGI", .type = DATA_TYPE_UINT64, },
		{ .name = "SERVER_PORT", .type = DATA_TYPE_UINT64, },
		{ .name = NULL, },
	};

	uint64_t intval;
	char *str;
	int ret;
	int i;

	for (i = 0; table[i].name; i++) {
		nvpair_t *pair;

		ret = nvlist_lookup_nvpair(req->request_headers,
					   table[i].name, &pair);
		if (ret)
			continue;

		if (nvpair_type(pair) != DATA_TYPE_STRING)
			continue;

		ret = nvpair_value_string(pair, &str);
		if (ret)
			continue;

		switch (table[i].type) {
			case DATA_TYPE_UINT64:
				ret = str2u64(str, &intval);
				if (!ret)
					nvl_set_int(req->request_headers,
						    table[i].name, intval);
				break;
			default:
				ASSERT(0);
		}
	}
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

	ret = nvlist_lookup_uint64(req->request_headers, "CONTENT_LENGTH",
				   &content_len);
	if (ret)
		return ret;

	if (!content_len)
		return 0;

	buf = malloc(content_len);
	if (!buf)
		return ENOMEM;

	ret = recv(req->scgi.fd, buf, content_len, MSG_WAITALL);
	ASSERT3S(ret, !=, -1);
	ASSERT3S(ret, ==, content_len);

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