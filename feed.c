#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "config.h"
#include "render.h"
#include "db.h"
#include "error.h"

static int __render_page(struct req *req, char *tmpl)
{
	printf("Content-Type: application/atom+xml; charset=UTF-8\n\n");
	printf("%s\n", render_page(req, tmpl));

	return 0;
}

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

		if (load_post(req, postid))
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
	req_head(req, "Content-Type", "application/atom+xml; charset=UTF-8");

	vars_scope_push(&req->vars);

	__load_posts(req, 0);

	return __render_page(req, "{feed}");
}

int blahg_feed(struct req *req, char *feed, int p)
{
	if (strcmp(feed, "atom"))
		disp_404("Atom only",
			 "When I first decided to write my own blogging "
			 "system, I made a decision that the only feed type "
			 "I'd support (at least for now) would be the Atom "
			 "format.  Yes, there are a lot of RSS and RSS2 feeds "
			 "out there, but it seems to be the case that Atom is "
			 "just as supported (and a bit easier to generate). "
			 "Feel free to contact me if you cannot live without "
			 "a non-Atom feed.");

	if (p != -1)
		disp_404("Comment feed not yet supported",
			 "It turns out that there are some features that I "
			 "never got around to implementing.  You just found "
			 "one of them.  Eventually, ");

	/* switch to atom */
	req->fmt = "atom";

	return __feed(req);
}
