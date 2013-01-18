#include <stdio.h>
#include <string.h>

#include "db.h"
#include "config.h"

sqlite3 *db;

int open_db()
{
	sqlite3_stmt *stmt;
	int ret;

	if (db)
		return 0;

	ret = sqlite3_open(DB_FILE, &db);
	if (ret != SQLITE_OK) {
		LOG("Database '%s' open failed: %s (%d)",
		    DB_FILE, sqlite3_errmsg(db), ret);
		return 1;
	}

	SQL(stmt, "PRAGMA foreign_keys = ON");
	SQL_RUN(stmt);

	return 0;
}

