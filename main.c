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
#include "qstring.h"
#include "val.h"

int main(int argc, char **argv)
{
	struct req req;
	int ret;

	openlog("blahg", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	init_val_subsys();
	init_pipe_subsys();

	req_init_cgi(&req);

	parse_query_string(req.request_qs, getenv("QUERY_STRING"));

#ifdef USE_XMLRPC
	req_head(&req, "X-Pingback", BASE_URL "/?xmlrpc=1");
#endif

	ret = req_dispatch(&req);

	req_output(&req);
	req_destroy(&req);

	return ret;
}
