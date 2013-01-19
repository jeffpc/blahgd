#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "main.h"
#include "render.h"
#include "sidebar.h"
#include "error.h"
#include "utils.h"

static int __render_page(struct req *req, char *tmpl)
{
	printf("Content-type: text/html\n\n");
	printf("%s\n", render_page(req, tmpl));

	return 0;
}

static void __store_title(struct vars *vars, const char *title)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

        vv.type = VT_STR;
        vv.str  = xstrdup(title);
        ASSERT(vv.str);

        ASSERT(!var_append(vars, "title", &vv));
}

static int __load_post(struct req *req, int p)
{
	int ret;

	ret = load_post(req, p);

	if (ret)
		__store_title(&req->vars, "not found");
	else
		__store_title(&req->vars, "STORY");

	return ret;
}

int blahg_story(struct req *req, int p)
{
	if (p == -1) {
		fprintf(stderr, "Invalid post #\n");
		return 0;
	}

	req_head(req, "Content-Type: text/html");

	sidebar(req);

	vars_scope_push(&req->vars);

	if (__load_post(req, p))
		return __render_page(req, "{404}");

	return __render_page(req, "{storyview}");
}
