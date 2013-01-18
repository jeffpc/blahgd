#ifndef __POST_H
#define __POST_H

#include <time.h>

#include "list.h"

struct post_tag {
	struct list_head list;
	char *tag;
};

struct comment {
	struct list_head list;
	int id;
	char *author;
	char *email;
	unsigned int time;
	char *ip;
	char *url;

	char *body;
};

struct post {
	/* from 'posts' table */
	int id;
	unsigned int time;
	char *title;
	int fmt;

	/* from 'post_tags' table */
	struct list_head tags;

	/* from 'comments' table */
	struct list_head comments;
	int numcom;

	/* body */
	char *body;
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

#define XATTR_TITLE		"user.post_title"
#define XATTR_CATS		"user.post_cats"
#define XATTR_TIME		"user.post_time"
#define XATTR_FMT		"user.post_fmt"
#define XATTR_COMM_AUTHOR	"user.author"
#define XATTR_COMM_EMAIL	"user.email"
#define XATTR_COMM_URL		"user.url"
#define XATTR_COMM_IP		"user.remote_addr"

struct req;

extern int load_post(struct req *req, int postid);
extern void dump_post(struct post_old *post);
extern void destroy_post(struct post *post);

#define max(a,b)	((a)<(b)? (b) : (a))

#endif
