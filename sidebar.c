#include <stdio.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "vars.h"
#include "db.h"
#include "sidebar.h"
#include "error.h"
#include "utils.h"

static struct var *__int_var(const char *name, uint64_t val)
{
	struct var *v;

	v = var_alloc(name);
	ASSERT(v);

	v->val[0].type = VT_INT;
	v->val[0].i    = val;

	return v;
}

static struct var *__str_var(const char *name, const char *val)
{
	struct var *v;

	v = var_alloc(name);
	ASSERT(v);

	v->val[0].type = VT_STR;
	v->val[0].str  = xstrdup(val);
	ASSERT(!val || v->val[0].str);

	return v;
}

static int __tag_size(int count, int cmin, int cmax)
{
	float size;

	if (count <= cmin)
		return TAGCLOUD_MIN_SIZE;

	size  = (TAGCLOUD_MAX_SIZE - TAGCLOUD_MIN_SIZE) * (count - cmin);
	size /= (float) (cmax - cmin);

	return ceil(TAGCLOUD_MIN_SIZE + size);
}

#define TAG_COUNTS	"SELECT tag, count(1) as c FROM post_tags GROUP BY tag HAVING c > 1 ORDER BY tag COLLATE NOCASE"
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
	SQL(stmt, "SELECT min(c), max(c) FROM (" TAG_COUNTS ")");
	SQL_FOR_EACH(stmt) {
		cmin = SQL_COL_INT(stmt, 0);
		cmax = SQL_COL_INT(stmt, 1);
	}

	SQL(stmt, TAG_COUNTS);
	SQL_FOR_EACH(stmt) {
		const char *tag;
		uint64_t count;
		uint64_t size;

		tag   = SQL_COL_STR(stmt, 0);
		count = SQL_COL_INT(stmt, 1);
		size  = __tag_size(count, cmin, cmax);

		vv.vars[0] = __str_var("name", tag);
		vv.vars[1] = __int_var("size", size);
		vv.vars[2] = __int_var("count", count);

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

		vv.vars[0] = __int_var("name", archid);
		vv.vars[1] = __str_var("desc", buf);

		ASSERT(!var_append(&req->vars, "archives", &vv));
	}
}

void sidebar(struct req *req)
{
	tagcloud(req);
	archive(req);
}
