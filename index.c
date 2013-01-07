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
#include "main.h"
#include "config_opts.h"
#include "sidebar.h"
#include "parse.h"
#include "render.h"
#include "db.h"

static int __render_page(struct req *req, char *tmpl)
{
	printf("Content-type: text/html\n\n");
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

static void __store_vars(struct vars *vars, int page)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_INT;
	vv.i    = page - 1;

	assert(!var_append(vars, "prevpage", &vv));

	vv.type = VT_INT;
	vv.i    = page + 1;

	assert(!var_append(vars, "nextpage", &vv));

        vv.type = VT_STR;
        vv.str  = strdup("Blahg");
        assert(vv.str);

        assert(!var_append(vars, "title", &vv));
}

int blahg_index(struct req *req, int page)
{
	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	__store_vars(&req->vars, page);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts(req, page);

	return __render_page(req, "{index}");
}
