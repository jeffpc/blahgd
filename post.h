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
	char *title;
	char *cats;
	struct tm time;
};

#define XATTR_TITLE		"user.post_title"
#define XATTR_CATS		"user.post_cats"
#define XATTR_TIME		"user.post_time"
#define XATTR_COMM_AUTHOR	"user.author"

extern int load_post(int postid, struct post *post);
extern void dump_post(struct post *post);
extern void destroy_post(struct post *post);

extern int load_comment(struct post *post, int commid, struct comment *comm);
extern void destroy_comment(struct comment *comm);

struct repltab_entry;

extern void cat_post(struct post *post);
extern void cat_post_comment(struct post *post, struct comment *comm);
extern void cat(struct post *post, struct comment *comm, char *tmpl,
		struct repltab_entry *repltab);

extern void invoke_for_each_comment(struct post *post,
				    void(*f)(struct post*, struct comment*));

#endif
