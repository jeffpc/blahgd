#ifndef __POST_H
#define __POST_H

#include <time.h>

struct comment {
	int id;
	char *author;
	struct tm time;
};

struct post {
	FILE *out;
	int id;
	int fmt;
	char *title;
	char *cats;
	struct tm time;
	int page;

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

extern int load_post(int postid, struct post *post);
extern void dump_post(struct post *post);
extern void destroy_post(struct post *post);

extern int load_comment(struct post *post, int commid, struct comment *comm);
extern void destroy_comment(struct comment *comm);

struct repltab_entry;

extern void cat_post(struct post *post);
extern void cat_post_comment(struct post *post, struct comment *comm);
extern void cat(struct post *post, void *data, char *tmpl,
		struct repltab_entry *repltab);

void __do_cat_post_fmt3(struct post *p, char *path);

extern void invoke_for_each_comment(struct post *post,
				    void(*f)(struct post*, struct comment*));

#define max(a,b)	((a)<(b)? (b) : (a))

static inline int tm_cmp(struct tm *t1, struct tm *t2)
{
	if (t1->tm_year < t2->tm_year)
		return -1;
	if (t1->tm_year > t2->tm_year)
		return 1;

	if (t1->tm_mon < t2->tm_mon)
		return -1;
	if (t1->tm_mon > t2->tm_mon)
		return 1;

	if (t1->tm_mday < t2->tm_mday)
		return -1;
	if (t1->tm_mday > t2->tm_mday)
		return 1;

	if (t1->tm_hour < t2->tm_hour)
		return -1;
	if (t1->tm_hour > t2->tm_hour)
		return 1;

	if (t1->tm_min < t2->tm_min)
		return -1;
	if (t1->tm_min > t2->tm_min)
		return 1;

	if (t1->tm_sec < t2->tm_sec)
		return -1;
	if (t1->tm_sec > t2->tm_sec)
		return 1;

	return 0;
}

#endif
