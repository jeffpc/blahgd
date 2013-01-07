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

	open_db();
	SQL(stmt, "SELECT id FROM posts ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_INT(stmt, 1, HTML_INDEX_STORIES);
	SQL_BIND_INT(stmt, 2, page * HTML_INDEX_STORIES);
	SQL_FOR_EACH(stmt) {
		int postid;

		postid = SQL_COL_INT(stmt, 0);

		if (load_post(req, postid))
			continue;
	}
}

static void __load_posts_archive(struct req *req, int page, int archid)
{
	char fromtime[32];
	char totime[32];
	sqlite3_stmt *stmt;
	int toyear, tomonth;
	int ret;

	toyear = archid / 100;
	tomonth = (archid % 100) + 1;
	if (tomonth > 12) {
		tomonth = 1;
		toyear++;
	}

	snprintf(fromtime, sizeof(fromtime), "%04d-%02d-01 00:00",
		 archid / 100, archid % 100);
	snprintf(totime, sizeof(totime), "%04d-%02d-01 00:00",
		 toyear, tomonth);

	open_db();
	SQL(stmt, "SELECT id FROM posts WHERE time>=? AND time<? ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_STR(stmt, 1, fromtime);
	SQL_BIND_STR(stmt, 2, totime);
	SQL_BIND_INT(stmt, 3, HTML_ARCHIVE_STORIES);
	SQL_BIND_INT(stmt, 4, page * HTML_ARCHIVE_STORIES);
	SQL_FOR_EACH(stmt) {
		int postid;

		postid = SQL_COL_INT(stmt, 0);

		if (load_post(req, postid))
			continue;
	}
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

static void __store_pages(struct vars *vars, int page)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_INT;
	vv.i    = page - 1;

	assert(!var_append(vars, "prevpage", &vv));

	vv.type = VT_INT;
	vv.i    = page + 1;

	assert(!var_append(vars, "nextpage", &vv));
}

static void __store_archid(struct vars *vars, int archid)
{
	struct var_val vv;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_INT;
	vv.i    = archid;

	assert(!var_append(vars, "archid", &vv));
}

int blahg_index(struct req *req, int page)
{
	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	__store_title(&req->vars, "Blahg");
	__store_pages(&req->vars, page);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts(req, page);

	return __render_page(req, "{index}");
}

static int validate_arch_id(int arch)
{
	int y = arch / 100;
	int m = arch % 100;

	return (m >= 1) && (m <= 12) && (y >= 1970) && (y < 2100);
}

int blahg_archive(struct req *req, int m, int page)
{
	static const char *months[12] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November",
		"December",
	};

	char nicetitle[32];

	if (!validate_arch_id(m))
		m = 197001;

	snprintf(nicetitle, sizeof(nicetitle), "%d &raquo; %s", m / 100,
		 months[(m % 100) - 1]);

	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	__store_title(&req->vars, nicetitle);
	__store_pages(&req->vars, page);
	__store_archid(&req->vars, m);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts_archive(req, page, m);

	return __render_page(req, "{archive}");
}
