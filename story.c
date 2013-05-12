#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "post.h"
#include "main.h"
#include "render.h"
#include "sidebar.h"
#include "error.h"
#include "utils.h"

static void __store_title(struct vars *vars, const char *title)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

        vv.type = VT_STR;
        vv.str  = xstrdup(title);
        ASSERT(vv.str);

        ASSERT(!var_append(vars, "title", &vv));
}

static int __load_post(struct req *req, int p, bool preview)
{
	int ret;

	ret = load_post(req, p, "title", preview);

	if (ret) {
		LOG("failed to load post #%d: %s (%d)%s", p, strerror(ret),
		    ret, preview ? " preview" : "");
		__store_title(&req->vars, "not found");
	}

	return ret;
}

int blahg_story(struct req *req, int p, bool preview)
{
	if (p == -1) {
		fprintf(stderr, "Invalid post #\n");
		return 0;
	}

	req_head(req, "Content-Type", "text/html");

	sidebar(req);

	vars_scope_push(&req->vars);

	if (__load_post(req, p, preview))
		return R404(req, NULL);

	req->body = render_page(req, "{storyview}");

	return 0;
}
