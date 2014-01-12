#ifndef __MAIN_H
#define __MAIN_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "post.h"
#include "vars.h"

enum {
	PAGE_MALFORMED,

	PAGE_ARCHIVE,
	PAGE_CATEGORY,
	PAGE_TAG,
	PAGE_COMMENT,
	PAGE_INDEX,
	PAGE_STORY,
	PAGE_XMLRPC,
	PAGE_ADMIN,
};

struct qs {
	int page;

	int p;
	int paged;
	int m;
	int preview;
	int xmlrpc;
	int admin;
	int comment;
	char *cat;
	char *tag;
	char *feed;
};

struct header {
	struct list_head list;
	char *name;
	char *val;
};

struct req {
	struct vars vars;

	unsigned int status;

	struct qs args;
	struct list_head headers;
	char *body;

	char *fmt;		/* format (e.g., "html") */

	struct {
		int index_stories;
	} opts;

	/* request latency calculation */
	bool dump_latency;
	uint64_t start;
};

extern void req_head(struct req *req, char *name, const char *val);

extern int blahg_archive(struct req *req, int m, int paged);
extern int blahg_category(struct req *req, char *cat, int page);
extern int blahg_tag(struct req *req, char *tag, int paged);
extern int blahg_comment(struct req *req);
extern int blahg_index(struct req *req, int paged);
extern int blahg_story(struct req *req, int p, bool preview);
extern int blahg_admin(struct req *req);
#ifdef USE_XMLRPC
extern int blahg_pingback();
#endif

extern int R404(struct req *req, char *tmpl);
extern int R301(struct req *req, const char *url);

static inline uint64_t gettime()
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

#endif
