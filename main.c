#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <syslog.h>

#include "decode.h"
#include "post.h"
#include "error.h"
#include "utils.h"
#include "render.h"
#include "sidebar.h"
#include "pipeline.h"
#include "req.h"
#include "val.h"
#include "template_cache.h"
#include "math.h"

static char *get_remote_addr()
{
	char *remote_addr;

	remote_addr = getenv("HTTP_X_REAL_IP");
	return remote_addr ? remote_addr : getenv("REMOTE_ADDR");
}

/*
 * Slurp up the environment into request headers.
 */
static void cgi_read_request(struct req *req)
{
	nvlist_t *nvl;

	nvl = nvl_alloc();

	nvl_set_str(nvl, CONTENT_LENGTH,	getenv("CONTENT_LENGTH"));
	nvl_set_str(nvl, CONTENT_TYPE,		getenv("CONTENT_TYPE"));
	nvl_set_str(nvl, DOCUMENT_URI,		getenv("DOCUMENT_URI"));
	nvl_set_str(nvl, HTTP_REFERER,		getenv("HTTP_REFERER"));
	nvl_set_str(nvl, HTTP_USER_AGENT,	getenv("HTTP_USER_AGENT"));
	nvl_set_str(nvl, QUERY_STRING,		getenv("QUERY_STRING"));
	nvl_set_str(nvl, REMOTE_ADDR,		get_remote_addr());
	nvl_set_str(nvl, REMOTE_PORT,		getenv("REMOTE_PORT"));
	nvl_set_str(nvl, REQUEST_METHOD,	getenv("REQUEST_METHOD"));
	nvl_set_str(nvl, REQUEST_URI,		getenv("REQUEST_URI"));
	nvl_set_str(nvl, SERVER_NAME,		getenv("SERVER_NAME"));
	nvl_set_str(nvl, SERVER_PORT,		getenv("SERVER_PORT"));
	nvl_set_str(nvl, SERVER_PROTOCOL,	getenv("SERVER_PROTOCOL"));

	/* CGI-only headers */
	nvl_set_str(nvl, "DOCUMENT_ROOT",	getenv("DOCUMENT_ROOT"));
	nvl_set_str(nvl, "PATH_INFO",		getenv("PATH_INFO"));
	nvl_set_str(nvl, "SCRIPT_NAME",		getenv("SCRIPT_NAME"));

	req->request_headers = nvl;

	convert_headers(req->request_headers, NULL);
}

int main(int argc, char **argv)
{
	struct req req;
	int ret;

	openlog("blahg", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	init_math(false);
	init_val_subsys();
	init_pipe_subsys();
	init_template_cache();

	init_db();

	req_init_cgi(&req);

	cgi_read_request(&req);

	ret = req_dispatch(&req);

	req_output(&req);
	req_destroy(&req);

	return ret;
}
