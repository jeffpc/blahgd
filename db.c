#include <stdio.h>

#include "db.h"
#include "config_opts.h"

sqlite3 *db;

int open_db()
{
	int ret;

	if (db)
		return 0;

	ret = sqlite3_open(DB_FILE, &db);
	if (ret != SQLITE_OK) {
		fprintf(stderr, "Database '%s' open failed: %s (%d)\n",
			DB_FILE, sqlite3_errmsg(db), ret);
		return 1;
	}

	return 0;
}

