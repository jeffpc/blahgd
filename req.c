#include <unistd.h>

#include "req.h"
#include "utils.h"

static void req_init(struct req *req, enum req_via via)
{
	/* state */
	req->dump_latency = true;
	req->start = gettime();

	vars_init(&req->vars);
	vars_set_str(&req->vars, "baseurl", BASE_URL);
	vars_set_int(&req->vars, "now", gettime());
	vars_set_int(&req->vars, "captcha_a", COMMENT_CAPTCHA_A);
	vars_set_int(&req->vars, "captcha_b", COMMENT_CAPTCHA_B);
	vars_set_nvl_array(&req->vars, "posts", NULL, 0);

	req->fmt   = NULL;

	/* request */
	req->via = via;
	req->request_headers = NULL;
	req->request_body = NULL;

	/* response */
	req->status = 200;
	req->headers = nvl_alloc();
	req->body  = NULL;
}

void req_init_cgi(struct req *req)
{
	req_init(req, REQ_CGI);
}

void req_init_scgi(struct req *req, int fd)
{
	req_init(req, REQ_SCGI);

	req->scgi.fd = fd;
}

void req_output(struct req *req)
{
	nvpair_t *header;

	printf("Status: %u\n", req->status);

	for (header = nvlist_next_nvpair(req->headers, NULL);
	     header;
	     header = nvlist_next_nvpair(req->headers, header))
		printf("%s: %s\n", nvpair_name(header), pair2str(header));

	printf("\n%s\n", req->body);

	if (req->dump_latency) {
		uint64_t delta;

		delta = gettime() - req->start;

		printf("<!-- time to render: %lu.%09lu seconds -->\n",
		       delta / 1000000000UL,
		       delta % 1000000000UL);
	}
}

void req_destroy(struct req *req)
{
	vars_destroy(&req->vars);

	nvlist_free(req->headers);

	free(req->request_body);
	nvlist_free(req->request_headers);

	switch (req->via) {
		case REQ_CGI:
			break;
		case REQ_SCGI:
			close(req->scgi.fd);
			break;
	}

	free(req->body);
}

void req_head(struct req *req, const char *name, const char *val)
{
	nvl_set_str(req->headers, name, val);
}
