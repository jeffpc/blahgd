#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "config.h"
#include "render.h"
#include "db.h"
#include "error.h"

static void __load_posts(struct req *req, int page)
{
	struct var_val vv;
	sqlite3_stmt *stmt;
	time_t maxtime;
	int ret;

	maxtime = 0;

	open_db();
	SQL(stmt, "SELECT id, strftime(\"%s\", time) FROM posts ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_INT(stmt, 1, FEED_INDEX_STORIES);
	SQL_BIND_INT(stmt, 2, page * FEED_INDEX_STORIES);
	SQL_FOR_EACH(stmt) {
		time_t posttime;
		int postid;

		postid   = SQL_COL_INT(stmt, 0);
		posttime = SQL_COL_INT(stmt, 1);

		if (load_post(req, postid, NULL, false))
			continue;

		if (posttime > maxtime)
			maxtime = posttime;
	}

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_INT;
	vv.i    = maxtime;

	ASSERT(!var_append(&req->vars, "lastupdate", &vv));
}

static int __feed(struct req *req)
{
	if (!strcmp(req->fmt, "atom"))
		req_head(req, "Content-Type", "application/atom+xml");
	else if (!strcmp(req->fmt, "rss2"))
		req_head(req, "Content-Type", "application/rss+xml");

	vars_scope_push(&req->vars);

	__load_posts(req, 0);

	req->body = render_page(req, "{feed}");
	return 0;
}

int blahg_feed(struct req *req, char *feed, int p)
{
	if (strcmp(feed, "atom") &&
	    strcmp(feed, "rss2"))
		return R404(req, "{error_unsupported_feed_fmt}");

	if (p != -1)
		return R404(req, "{error_comment_feed}");

	/* switch to to the right type */
	req->fmt = feed;

	return __feed(req);
}
