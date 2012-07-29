#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"
#include "main.h"
#include "map.h"
#include "config_opts.h"

static char *__render(struct req *req, char *str)
{
}

int render_page(struct req *req, char *tmpl, char *fmt)
{
	int ret;

	ret = load_map(&req->map, fmt);
	if (ret)
		return ret;

	printf("%s\n", __render(req, strdup("{index}")));

	return ret;
}

int blahg_index(struct req *req, int page)
{
	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	return render_page(req, "index", "html");
}
