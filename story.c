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

static int __load_post(struct req *req, int p, bool preview)
{
	struct val *posts;
	struct val *post;

	post = load_post(req, p, "title", preview);
	if (!post) {
		LOG("failed to load post #%d: %s (%d)%s", p, "XXX",
		    -1, preview ? " preview" : "");

		VAR_SET_STR(&req->vars, "title", xstrdup("not found"));
	} else {
		posts = VAR_LOOKUP_VAL(&req->vars, "posts");

		VAL_SET_LIST(posts, 0, post);
	}

	return post ? 0 : ENOENT;
}

int blahg_story(struct req *req, int p, bool preview)
{
	if (p == -1) {
		fprintf(stderr, "Invalid post #\n");
		return 0;
	}

	sidebar(req);

	vars_scope_push(&req->vars);

	if (__load_post(req, p, preview))
		return R404(req, NULL);

	req->body = render_page(req, "{storyview}");

	return 0;
}
