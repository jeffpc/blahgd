/*
 * Copyright (c) 2009-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
#include "req.h"
#include "config.h"
#include "sidebar.h"
#include "parse.h"
#include "render.h"
#include "db.h"
#include "error.h"
#include "utils.h"

static void __load_posts(struct req *req, int page)
{
	const unsigned int posts_per_page = req->opts.index_stories;
	struct post *posts[posts_per_page];
	int nposts;

	nposts = index_get_posts(posts, NULL, false, NULL, NULL,
				 page * posts_per_page, posts_per_page);

	load_posts(req, posts, nposts, nposts == posts_per_page);
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

	SQL(stmt, "SELECT id, strftime(\"%s\", time) FROM posts WHERE time>=? AND time<? ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_STR(stmt, 1, fromtime);
	SQL_BIND_STR(stmt, 2, totime);
	SQL_BIND_INT(stmt, 3, req->opts.index_stories);
	SQL_BIND_INT(stmt, 4, page * req->opts.index_stories);

	load_posts_sql(req, stmt, req->opts.index_stories);

	SQL_END(stmt);
}

static void __store_title(struct vars *vars, char *title)
{
	vars_set_str(vars, "title", title);
}

static void __store_pages(struct vars *vars, int page)
{
	vars_set_int(vars, "prevpage", page + 1);
	vars_set_int(vars, "curpage",  page);
	vars_set_int(vars, "nextpage", page - 1);
}

static void __store_archid(struct vars *vars, int archid)
{
	vars_set_int(vars, "archid", archid);
}

int blahg_index(struct req *req, int page)
{
	page = max(page, 0);

	__store_title(&req->vars, "Blahg");
	__store_pages(&req->vars, page);

	sidebar(req);

	vars_scope_push(&req->vars);

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

	vars_scope_push(&req->vars);

	__load_posts_archive(req, page, m);

	req->body = render_page(req, "{archive}");
	return 0;
}
