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

static void __load_posts_tag(struct req *req, int page, const char *tag,
			     bool istag, int nstories)
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

		if (load_post(req, postid, NULL))
			continue;
	}
}

int __tagcat(struct req *req, const char *tagcat, int page, char *tmpl,
	     bool istag, int nstories)
{
	if (!tagcat)
		return R404(req, NULL);

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
	int catn;

	/* wordpress cat name */
	catn = atoi(cat);
	if (catn && ((catn < MIN_CATN) || (catn > MAX_CATN)))
		return R404(req, NULL);

	return __tagcat(req, catn ? wordpress_catn[catn] : cat, page,
			"{catindex}", false, HTML_CATEGORY_STORIES);
}
