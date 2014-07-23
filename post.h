#ifndef __POST_H
#define __POST_H

#include <time.h>
#include <stdbool.h>
#include <sys/list.h>

#include "db.h"
#include "vars.h"

struct post_tag {
	list_node_t list;
	char *tag;
};

struct comment {
	list_node_t list;
	unsigned int id;
	char *author;
	char *email;
	unsigned int time;
	char *ip;
	char *url;

	char *body;
};

struct post {
	/* from 'posts' table */
	unsigned int id;
	unsigned int time;
	char *title;
	unsigned int fmt;

	/* from 'post_tags' table */
	list_t tags;

	/* from 'comments' table */
	list_t comments;
	unsigned int numcom;

	/* body */
	struct str *body;

	/* fmt3 */
	int table_nesting;
	int texttt_nesting;
};

struct post_old {
	int id;
	int fmt;
	char *title;
	char *cats, *tags;
	struct tm time;
	int page;

	char *path;
	int preview;

	char *pagetype; /* date for archive pages, category name for
			   category lists, undefined otherwise */
	struct tm lasttime; /* date/time of the last update */
};

struct req;

extern nvlist_t *load_post(struct req *req, int postid, const char *titlevar, bool preview);
extern void dump_post(struct post_old *post);
extern void destroy_post(struct post *post);
extern void load_posts(struct req *req, sqlite3_stmt *stmt);

#define max(a,b)	((a)<(b)? (b) : (a))

#endif
