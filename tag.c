#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "config.h"
#include "db.h"
#include "render.h"
#include "main.h"
#include "utils.h"
#include "sidebar.h"
#include "error.h"

static void __store_title(struct vars *vars, const char *title)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

        vv.type = VT_STR;
        vv.str  = xstrdup(title);
        ASSERT(vv.str);

        ASSERT(!var_append(vars, "title", &vv));
}

static void __store_tag(struct vars *vars, const char *tag)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

        vv.type = VT_STR;
        vv.str  = xstrdup(tag);
        ASSERT(vv.str);

        ASSERT(!var_append(vars, "tagid", &vv));
}

static void __store_pages(struct vars *vars, int page)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_INT;
	vv.i    = page + 1;

	ASSERT(!var_append(vars, "prevpage", &vv));

	vv.type = VT_INT;
	vv.i    = page - 1;

	ASSERT(!var_append(vars, "nextpage", &vv));
}

static void __load_posts_tag(struct req *req, int page, char *tag, bool istag,
			     int nstories)
{
	sqlite3_stmt *stmt;
	int ret;

	open_db();
	if (istag)
		SQL(stmt, "SELECT post_tags.post FROM post_tags,posts "
		    "WHERE post_tags.post=posts.id AND post_tags.tag=? "
		    "ORDER BY posts.time DESC LIMIT ? OFFSET ?");
	else
		SQL(stmt, "SELECT post_cats.post FROM post_cats,posts "
		    "WHERE post_cats.post=posts.id AND post_cats.cat=? "
		    "ORDER BY posts.time DESC LIMIT ? OFFSET ?");
	SQL_BIND_STR(stmt, 1, tag);
	SQL_BIND_INT(stmt, 2, nstories);
	SQL_BIND_INT(stmt, 3, page * nstories);
	SQL_FOR_EACH(stmt) {
		int postid;

		postid = SQL_COL_INT(stmt, 0);

		if (load_post(req, postid))
			continue;
	}
}

int __tagcat(struct req *req, char *tagcat, int page, char *tmpl, bool istag,
	     int nstories)
{
	if (!tagcat) {
		printf("Content-type: text/plain\n\n");
		printf("Invalid tag/category name\n");
		return 0;
	}

	page = max(page, 0);

	__store_title(&req->vars, tagcat);
	__store_pages(&req->vars, page);
	__store_tag(&req->vars, tagcat);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts_tag(req, page, tagcat, istag, nstories);

	req->body = render_page(req, tmpl);
	return 0;
}

int blahg_tag(struct req *req, char *tag, int page)
{
	return __tagcat(req, tag, page, "{tagindex}", true, HTML_TAG_STORIES);
}

int blahg_category(struct req *req, char *cat, int page)
{
	return __tagcat(req, cat, page, "{catindex}", false,
			HTML_CATEGORY_STORIES);
}
