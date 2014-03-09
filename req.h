#ifndef __REQ_H
#define __REQ_H

#include <libnvpair.h>
#include <stdbool.h>

#include "list.h"
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
			struct list_head queue;
			int fd;
		} scgi;
	};

	/* request */
	enum req_via via;
	nvlist_t *request_headers;
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
};

extern void req_init_scgi(struct req *req, int fd);
extern void req_init_cgi(struct req *req);
extern void req_destroy(struct req *req);
extern void req_output(struct req *req);
extern void req_head(struct req *req, const char *name, const char *val);

#endif
