#include <stdio.h>
#include <string.h>
#include <math.h>

#include "req.h"
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
	sqlite3_stmt *stmt;
	nvlist_t **cloud;
	int cmin, cmax;
	int nitems;
	int ret;
	int i;

	i = 0;

	/* pacify gcc */
	cmin = 0;
	cmax = 0;
	nitems = 0;

	SQL(stmt, "SELECT min(cnt), max(cnt), count(1) FROM tagcloud");
	SQL_FOR_EACH(stmt) {
		cmin   = SQL_COL_INT(stmt, 0);
		cmax   = SQL_COL_INT(stmt, 1);
		nitems = SQL_COL_INT(stmt, 2);
	}

	SQL_END();

	/*
	 * allocate enough space for every tag, even if we don't care about
	 * the ones with <2 count
	 */
	cloud = malloc(sizeof(nvlist_t *) * nitems);
	ASSERT(cloud);

	SQL(stmt, "SELECT tag, cnt FROM tagcloud WHERE cnt > 1 ORDER BY tag COLLATE NOCASE");
	SQL_FOR_EACH(stmt) {
		char *tag;
		uint64_t count;
		uint64_t size;

		tag   = SQL_COL_STR(stmt, 0);
		count = SQL_COL_INT(stmt, 1);
		size  = __tag_size(count, cmin, cmax);

		cloud[i] = nvl_alloc();

		ASSERT3U(i, <, nitems);

		nvl_set_str(cloud[i], "name", tag);
		nvl_set_int(cloud[i], "size", size);
		nvl_set_int(cloud[i], "count", count);

		i++;
	}

	SQL_END();

	vars_set_nvl_array(&req->vars, "tagcloud", cloud, i);

	for (nitems = i, i = 0; i < nitems; i++)
		nvlist_free(cloud[i]);

	free(cloud);
}

static void archive(struct req *req)
{
	static const char *months[12] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November",
		"December",
	};

	sqlite3_stmt *stmt;
	nvlist_t **archives;
	uint_t narchives;
	uint_t i;
	int ret;

	archives = NULL;
	narchives = 0;

	SQL(stmt, "SELECT DISTINCT STRFTIME(\"%Y%m\", time) AS t FROM posts ORDER BY t DESC");
	SQL_FOR_EACH(stmt) {
		char buf[32];
		int archid;

		archid = SQL_COL_INT(stmt, 0);

		snprintf(buf, sizeof(buf), "%s %d", months[(archid % 100) - 1],
			 archid / 100);

		archives = realloc(archives, sizeof(nvlist_t *) * (narchives + 1));
		ASSERT(archives);

		archives[narchives] = nvl_alloc();

		nvl_set_int(archives[narchives], "name", archid);
		nvl_set_str(archives[narchives], "desc", buf);

		narchives++;
	}

	SQL_END();

	vars_set_nvl_array(&req->vars, "archives", archives, narchives);

	for (i = 0; i < narchives; i++)
		nvlist_free(archives[i]);

	free(archives);
}

void sidebar(struct req *req)
{
	tagcloud(req);
	archive(req);
}
