#include <stdio.h>
#include <string.h>

#include "db.h"
#include "config.h"

sqlite3 *db;
pthread_mutex_t db_lock;

void init_db()
{
	pthread_mutexattr_t attr;

	ASSERT0(pthread_mutexattr_init(&attr));
	ASSERT0(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));

	ASSERT0(pthread_mutex_init(&db_lock, &attr));

	ASSERT0(pthread_mutexattr_destroy(&attr));
}

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
	SQL_END();

	return 0;
}
