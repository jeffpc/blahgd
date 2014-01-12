#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "post.h"
#include "main.h"
#include "config.h"
#include "sidebar.h"
#include "parse.h"
#include "render.h"
#include "db.h"
#include "error.h"
#include "utils.h"

static void __load_posts(struct req *req, int page)
{
	sqlite3_stmt *stmt;
	struct val *posts;
	struct val *val;
	time_t maxtime;
	int ret;
	int i;

	maxtime = 0;
	i = 0;

	posts = VAR_LOOKUP_VAL(&req->vars, "posts");

	open_db();
	SQL(stmt, "SELECT id, strftime(\"%s\", time) FROM posts ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_INT(stmt, 1, req->opts.index_stories);
	SQL_BIND_INT(stmt, 2, page * req->opts.index_stories);
	SQL_FOR_EACH(stmt) {
		time_t posttime;
		int postid;

		postid   = SQL_COL_INT(stmt, 0);
		posttime = SQL_COL_INT(stmt, 1);

		val = load_post(req, postid, NULL, false);
		if (!val)
			continue;

		VAL_SET_LIST(posts, i++, val);

		if (posttime > maxtime)
			maxtime = posttime;
	}

	val_putref(posts);

	VAR_SET_INT(&req->vars, "lastupdate", maxtime);
}

static void __load_posts_archive(struct req *req, int page, int archid)
{
	char fromtime[32];
	char totime[32];
	sqlite3_stmt *stmt;
	struct val *posts;
	struct val *val;
	time_t maxtime;
	int toyear, tomonth;
	int ret;
	int i;

	toyear = archid / 100;
	tomonth = (archid % 100) + 1;
	if (tomonth > 12) {
		tomonth = 1;
		toyear++;
	}

	maxtime = 0;
	i = 0;

	posts = VAR_LOOKUP_VAL(&req->vars, "posts");

	snprintf(fromtime, sizeof(fromtime), "%04d-%02d-01 00:00",
		 archid / 100, archid % 100);
	snprintf(totime, sizeof(totime), "%04d-%02d-01 00:00",
		 toyear, tomonth);

	open_db();
	SQL(stmt, "SELECT id, strftime(\"%s\", time) FROM posts WHERE time>=? AND time<? ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_STR(stmt, 1, fromtime);
	SQL_BIND_STR(stmt, 2, totime);
	SQL_BIND_INT(stmt, 3, HTML_ARCHIVE_STORIES);
	SQL_BIND_INT(stmt, 4, page * HTML_ARCHIVE_STORIES);
	SQL_FOR_EACH(stmt) {
		time_t posttime;
		int postid;

		postid   = SQL_COL_INT(stmt, 0);
		posttime = SQL_COL_INT(stmt, 1);

		val = load_post(req, postid, NULL, false);
		if (!val)
			continue;

		VAL_SET_LIST(posts, i++, val);

		if (posttime > maxtime)
			maxtime = posttime;
	}

	val_putref(posts);

	VAR_SET_INT(&req->vars, "lastupdate", maxtime);
}

static void __store_title(struct vars *vars, char *title)
{
	VAR_SET_STR(vars, "title", xstrdup(title));
}

static void __store_pages(struct vars *vars, int page)
{
	VAR_SET_INT(vars, "prevpage", page + 1);
	VAR_SET_INT(vars, "nextpage", page - 1);
}

static void __store_archid(struct vars *vars, int archid)
{
	VAR_SET_INT(vars, "archid", archid);
}

int blahg_index(struct req *req, int page)
{
	page = max(page, 0);

	__store_title(&req->vars, "Blahg");
	__store_pages(&req->vars, page);

	sidebar(req);

	__load_posts(req, page);

	req->body = render_page(req, "{index}");
	return 0;
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

	req_head(req, "Content-Type", "text/html");

	page = max(page, 0);

	__store_title(&req->vars, nicetitle);
	__store_pages(&req->vars, page);
	__store_archid(&req->vars, m);

	sidebar(req);

	__load_posts_archive(req, page, m);

	req->body = render_page(req, "{archive}");
	return 0;
}
