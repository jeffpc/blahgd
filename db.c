/*
 * Copyright (c) 2012-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
#include <string.h>

#include "db.h"
#include "config.h"

sqlite3 *db;
pthread_mutex_t db_lock;

void init_db(void)
{
	pthread_mutexattr_t attr;

	ASSERT0(pthread_mutexattr_init(&attr));
	ASSERT0(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));

	ASSERT0(pthread_mutex_init(&db_lock, &attr));

	ASSERT0(pthread_mutexattr_destroy(&attr));
}

int open_db(void)
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
	SQL_END(stmt);

	return 0;
}
