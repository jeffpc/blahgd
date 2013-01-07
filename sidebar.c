#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "main.h"
#include "vars.h"
#include "db.h"
#include "sidebar.h"

static struct var *__int_var(const char *name, uint64_t val)
{
	struct var *v;

	v = var_alloc(name);
	assert(v);

	v->val[0].type = VT_INT;
	v->val[0].i    = val;

	return v;
}

static struct var *__str_var(const char *name, const char *val)
{
	struct var *v;

	v = var_alloc(name);
	assert(v);

	v->val[0].type = VT_STR;
	v->val[0].str  = val ? strdup(val) : NULL;
	assert(!val || v->val[0].str);

	return v;
}

static void tagcloud(struct req *req)
{
	struct var_val vv;
	sqlite3_stmt *stmt;
	int ret;

	memset(&vv, 0, sizeof(vv));

	vv.type = VT_VARS;

	open_db();
	SQL(stmt, "SELECT tag, count(1) FROM post_tags GROUP BY tag ORDER BY tag");
	SQL_FOR_EACH(stmt) {
		const char *tag;
		uint64_t size;

		tag  = SQL_COL_STR(stmt, 0);
		size = SQL_COL_INT(stmt, 1) * 3;

		vv.vars[0] = __str_var("name", tag);
		vv.vars[1] = __int_var("size", size);

		assert(!var_append(&req->vars, "tags", &vv));
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

		assert(!var_append(&req->vars, "archives", &vv));
	}
}

void sidebar(struct req *req)
{
	tagcloud(req);
	archive(req);
}
