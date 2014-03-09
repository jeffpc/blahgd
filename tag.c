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

static const char *wordpress_catn[] = {
	[1]  = "miscellaneous",
	[2]  = "programming/kernel",
	[3]  = "school",
	[4]  = "work",
	[5]  = "random",
	[6]  = "programming",
	[7]  = "events/ols-2005",
	[8]  = "events",
	[9]  = "rants",
	[10] = "movies",
	[11] = "humor",
	[13] = "star-trek",
	[15] = "star-trek/tng",
	[16] = "star-trek/tos",
	[17] = "legal",
	[18] = "star-trek/voy",
	[19] = "star-trek/ent",
	[20] = "events/ols-2006",
	[21] = "fsl",
	[22] = "fsl/unionfs",
	[23] = "stargate/sg-1",
	[24] = "open-source",
	[25] = "astronomy",
	[26] = "programming/vcs",
	[27] = "programming/vcs/git",
	[28] = "programming/vcs/mercurial",
	[29] = "events/ols-2007",
	[30] = "programming/vcs/guilt",
	[31] = "photography",
	[34] = "music",
	[35] = "programming/mainframes",
	[36] = "events/sc-07",
	[39] = "hvf",
	[40] = "events/ols-2008",
	[41] = "sysadmin",
	[42] = "documentation",
	[43] = "stargate",
};

#define MIN_CATN 1
#define MAX_CATN 43

static void __store_title(struct vars *vars, const char *title)
{
	vars_set_str(vars, "title", xstrdup(title));
}

static void __store_tag(struct vars *vars, const char *tag)
{
	vars_set_str(vars, "tagid", xstrdup(tag));
}

static void __store_pages(struct vars *vars, int page)
{
	vars_set_int(vars, "prevpage", page + 1);
	vars_set_int(vars, "nextpage", page - 1);
}

static void __load_posts_tag(struct req *req, int page, const char *tag,
			     bool istag)
{
	const char *tc = istag ? "tag" : "cat";
	sqlite3_stmt *stmt;
	char sql[256];

	snprintf(sql, sizeof(sql), "SELECT post_%ss.post, strftime(\"%%s\", time) "
		 "FROM post_%ss,posts "
		 "WHERE post_%ss.post=posts.id AND post_%ss.%s=? "
		 "ORDER BY posts.time DESC LIMIT ? OFFSET ?", tc, tc, tc,
		 tc, tc);

	SQL(stmt, sql);
	SQL_BIND_STR(stmt, 1, tag);
	SQL_BIND_INT(stmt, 2, req->opts.index_stories);
	SQL_BIND_INT(stmt, 3, page * req->opts.index_stories);

	load_posts(req, stmt);
}

int __tagcat(struct req *req, const char *tagcat, int page, char *tmpl,
	     bool istag)
{
	if (!tagcat)
		return R404(req, NULL);

	req_head(req, "Content-Type", "text/html");

	page = max(page, 0);

	__store_title(&req->vars, tagcat);
	__store_pages(&req->vars, page);
	__store_tag(&req->vars, tagcat);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts_tag(req, page, tagcat, istag);

	req->body = render_page(req, tmpl);
	return 0;
}

int blahg_tag(struct req *req, char *tag, int page)
{
	return __tagcat(req, tag, page, "{tagindex}", true);
}

int blahg_category(struct req *req, char *cat, int page)
{
	int catn;

	/* wordpress cat name */
	catn = atoi(cat);
	if (catn && ((catn < MIN_CATN) || (catn > MAX_CATN)))
		return R404(req, NULL);

	if (catn) {
		char url[256];

		snprintf(url, sizeof(url), "%s/?cat=%s", BASE_URL,
			 wordpress_catn[catn]);

		return R301(req, url);
	}

	return __tagcat(req, cat, page, "{catindex}", false);
}
