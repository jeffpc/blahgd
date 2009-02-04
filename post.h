#ifndef __POST_H
#define __POST_H

#include <time.h>

struct post {
	FILE *out;
	int id;
	char *title;
	char *cats;
	struct tm time;
};

#define XATTR_TITLE	"user.post_title"
#define XATTR_CATS	"user.post_cats"
#define XATTR_TIME	"user.post_time"

extern int load_post(int postid, struct post *post);
extern void dump_post(struct post *post);
extern void destroy_post(struct post *post);

struct repltab_entry;

extern void cat_post(struct post *post);
extern void cat(struct post *post, char *tmpl, struct repltab_entry *repltab);

#endif
