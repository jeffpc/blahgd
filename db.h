#ifndef __DB_H
#define __DB_H

#include <sqlite3.h>

extern sqlite3 *db;

extern int open_db();

#define SQL(s, sql)	\
	do { \
		int ret; \
		ret = sqlite3_prepare_v2(db, (sql), strlen(sql) + 1, &(s), NULL); \
		if (ret != SQLITE_OK) { \
			fprintf(stderr, "Error %s:%d: %s (%d)\n", \
				__FILE__, __LINE__, sqlite3_errmsg(db), \
				ret); \
			return; \
		} \
	} while(0)

#define SQL_BIND_INT(s, i, v)	\
	do { \
		int ret; \
		ret = sqlite3_bind_int((s), (i), (v)); \
		if (ret != SQLITE_OK) { \
			fprintf(stderr, "Error %s:%d: %s (%d)\n", \
				__FILE__, __LINE__, sqlite3_errmsg(db), \
				ret); \
			return; \
		} \
	} while(0)

#define SQL_BIND_STR(s, i, v)	\
	do { \
		int ret; \
		ret = sqlite3_bind_text((s), (i), (v), strlen(v), SQLITE_TRANSIENT); \
		if (ret != SQLITE_OK) { \
			fprintf(stderr, "Error %s:%d: %s (%d)\n", \
				__FILE__, __LINE__, sqlite3_errmsg(db), \
				ret); \
			return; \
		} \
	} while(0)

#define SQL_RUN(s)	\
	do { \
		int ret; \
		ret = sqlite3_step(s); \
		if (ret != SQLITE_DONE) { \
			fprintf(stderr, "Error %s:%d: %s (%d)\n", \
				__FILE__, __LINE__, sqlite3_errmsg(db), \
				ret); \
			return; \
		} \
	} while(0)

#define SQL_FOR_EACH(s)	\
	for (; ret = sqlite3_step(s), ret == SQLITE_ROW; )

#define SQL_COL_INT(s, i)	\
	sqlite3_column_int((s), (i))

#define SQL_COL_STR(s, i)	\
	sqlite3_column_text((s), (i))

#define SQL_COL_STR_LEN(s, i)	\
	sqlite3_column_bytes((s), (i))

#endif
