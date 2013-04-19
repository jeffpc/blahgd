#include <stdio.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "vars.h"
#include "db.h"
#include "sidebar.h"
#include "error.h"
#include "utils.h"

static int __tag_size(int count, int cmin, int cmax)
{
	float size;

	if (count <= cmin)
		return TAGCLOUD_MIN_SIZE;

	size  = (TAGCLOUD_MAX_SIZE - TAGCLOUD_MIN_SIZE) * (count - cmin);
	size /= (float) (cmax - cmin);

	return ceil(TAGCLOUD_MIN_SIZE + size);
}

static void tagcloud(struct req *req)
{
	struct var_val vv;
	sqlite3_stmt *stmt;
	int cmin, cmax;
	int ret;

	/* pacify gcc */
	cmin = 0;
	cmax = 0;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_VARS;

	open_db();
	SQL(stmt, "SELECT min(cnt), max(cnt) FROM tagcloud");
	SQL_FOR_EACH(stmt) {
		cmin = SQL_COL_INT(stmt, 0);
		cmax = SQL_COL_INT(stmt, 1);
	}

	SQL(stmt, "SELECT tag, cnt FROM tagcloud WHERE cnt > 1 ORDER BY tag COLLATE NOCASE");
	SQL_FOR_EACH(stmt) {
		const char *tag;
		uint64_t count;
		uint64_t size;

		tag   = SQL_COL_STR(stmt, 0);
		count = SQL_COL_INT(stmt, 1);
		size  = __tag_size(count, cmin, cmax);

		vv.vars[0] = VAR_ALLOC_STR("name", tag);
		vv.vars[1] = VAR_ALLOC_INT("size", size);
		vv.vars[2] = VAR_ALLOC_INT("count", count);

		ASSERT(!var_append(&req->vars, "tagcloud", &vv));
	}
}

static void archive(struct req *req)
{
	static const char *months[12] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November",
		"December",
	};

	struct var_val vv;
	sqlite3_stmt *stmt;
	int ret;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_VARS;

	open_db();
	SQL(stmt, "SELECT DISTINCT STRFTIME(\"%Y%m\", time) AS t FROM posts ORDER BY t DESC");
	SQL_FOR_EACH(stmt) {
		char buf[32];
		int archid;

		archid = SQL_COL_INT(stmt, 0);

		snprintf(buf, sizeof(buf), "%s %d", months[(archid % 100) - 1],
			 archid / 100);

		vv.vars[0] = VAR_ALLOC_INT("name", archid);
		vv.vars[1] = VAR_ALLOC_STR("desc", buf);

		ASSERT(!var_append(&req->vars, "archives", &vv));
	}
}

void sidebar(struct req *req)
{
	tagcloud(req);
	archive(req);
}