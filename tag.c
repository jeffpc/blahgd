#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "config.h"
#include "db.h"
#include "render.h"
#include "main.h"
#include "utils.h"
#include "sidebar.h"

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
        vv.str  = strdup(title);
        assert(vv.str);

        assert(!var_append(vars, "title", &vv));
}

static void __store_tag(struct vars *vars, const char *tag)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

        vv.type = VT_STR;
        vv.str  = strdup(tag);
        assert(vv.str);

        assert(!var_append(vars, "tagid", &vv));
}

static void __store_pages(struct vars *vars, int page)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_INT;
	vv.i    = page + 1;

	assert(!var_append(vars, "prevpage", &vv));

	vv.type = VT_INT;
	vv.i    = page - 1;

	assert(!var_append(vars, "nextpage", &vv));
}

static void __load_posts_tag(struct req *req, int page, char *tag)
{
	sqlite3_stmt *stmt;
	int ret;

	open_db();
	SQL(stmt, "SELECT post_tags.post FROM post_tags,posts "
	    "WHERE post_tags.post=posts.id AND post_tags.tag=? "
	    "ORDER BY posts.time DESC LIMIT ? OFFSET ?");
	SQL_BIND_STR(stmt, 1, tag);
	SQL_BIND_INT(stmt, 2, HTML_TAG_STORIES);
	SQL_BIND_INT(stmt, 3, page * HTML_TAG_STORIES);
	SQL_FOR_EACH(stmt) {
		int postid;

		postid = SQL_COL_INT(stmt, 0);

		if (load_post(req, postid))
			continue;
	}
}

int blahg_tag(struct req *req, char *tag, int page)
{
	if (!tag) {
		fprintf(stdout, "Invalid tag name\n");
		return 0;
	}

	/*
	 * SECURITY CHECK: make sure no one is trying to give us a '..' in
	 * the path
	 */
	if (hasdotdot(tag)) {
		fprintf(stdout, "Go away\n");
		return 0;
	}

	page = max(page, 0);

	__store_title(&req->vars, tag);
	__store_pages(&req->vars, page);
	__store_tag(&req->vars, tag);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts_tag(req, page, tag);

	return __render_page(req, "{tagindex}");
}
