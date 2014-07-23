#ifndef __REQ_H
#define __REQ_H

#include <libnvpair.h>
#include <stdbool.h>
#include <sys/list.h>

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

enum req_via {
	REQ_CGI,
	REQ_SCGI,
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

struct req {
	union {
		struct {
			list_node_t queue;
			int fd;
		} scgi;
	};

	/* request */
	enum req_via via;
	nvlist_t *request_headers;
	nvlist_t *request_qs;
	char *request_body;
	struct qs args;

	/* response */
	unsigned int status;
	nvlist_t *headers;
	char *body;

	/* state */
	bool dump_latency;	/* request latency calculation */
	uint64_t start;

	struct vars vars;

	char *fmt;		/* format (e.g., "html") */

	struct {
		int index_stories;
	} opts;

	FILE *out;
};

extern void req_init_scgi(struct req *req, int fd);
extern void req_init_cgi(struct req *req);
extern void req_destroy(struct req *req);
extern void req_output(struct req *req);
extern void req_head(struct req *req, const char *name, const char *val);
extern int req_dispatch(struct req *req);

extern int R404(struct req *req, char *tmpl);
extern int R301(struct req *req, const char *url);

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

#endif
