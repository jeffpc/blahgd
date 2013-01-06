#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "post.h"
#include "sar.h"
#include "html.h"
#include "main.h"
#include "config_opts.h"
#include "parse.h"
#include "db.h"

static int __render_page(struct req *req, char *tmpl)
{
	printf("%s\n", render_page(req, tmpl));

	return 0;
}

static void __load_posts(struct req *req, int page)
{
	sqlite3_stmt *stmt;
	int ret;

	req->u.index.nposts = 0;

	open_db();
	SQL(stmt, "SELECT id FROM posts ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_INT(stmt, 1, HTML_INDEX_STORIES);
	SQL_BIND_INT(stmt, 2, page * HTML_INDEX_STORIES);
	SQL_FOR_EACH(stmt) {
		int postid;

		postid = SQL_COL_INT(stmt, 0);

		if (load_post(req, postid))
			continue;

		req->u.index.nposts++;
	}
}

int blahg_index(struct req *req, int page)
{
	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	vars_scope_push(&req->vars);

	__load_posts(req, page);

	vars_dump(&req->vars);

	return __render_page(req, "{index}");
}
