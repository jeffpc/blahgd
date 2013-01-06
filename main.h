#ifndef __MAIN_H
#define __MAIN_H

#include <time.h>

#include "avl.h"
#include "config_opts.h"
#include "post.h"
#include "vars.h"

enum {
	PAGE_MALFORMED,

	PAGE_ARCHIVE,
	PAGE_CATEGORY,
	PAGE_TAG,
	PAGE_COMMENT,
	PAGE_FEED,
	PAGE_INDEX,
	PAGE_STORY,
	PAGE_XMLRPC,
};

struct qs {
	int page;

	int p;
	int paged;
	int m;
	int preview;
	int xmlrpc;
	char *cat;
	char *tag;
	char *feed;
	char *comment;
};

struct req {
	struct timespec start;

	struct vars vars;

	struct qs args;
	char *buf;
	char *head;

	char *fmt;		/* format (e.g., "html") */

	union {
		struct {
			int nposts;
			struct post posts[HTML_INDEX_STORIES];
		} index;
	} u;
};

extern void req_head(struct req *req, char *header);

extern int blahg_archive(int m, int paged);
extern int blahg_category(char *cat, int paged);
extern int blahg_tag(char *tag, int paged);
extern int blahg_comment();
extern int blahg_feed(char *feed, int p);
extern int blahg_index(struct req *req, int paged);
extern int blahg_story(int p, int preview);
#ifdef USE_XMLRPC
extern int blahg_pingback();
#endif

extern void disp_404(char *title, char *txt);

#endif
