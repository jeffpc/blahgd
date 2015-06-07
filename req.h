#ifndef __REQ_H
#define __REQ_H

#include <libnvpair.h>
#include <stdbool.h>
#include <sys/list.h>

#include "vars.h"

/*
 * Defines to make it easier to keep track of required headers in
 * req->request_headers.
 */
#define CONTENT_LENGTH		"CONTENT_LENGTH"
#define CONTENT_TYPE		"CONTENT_TYPE"
#define DOCUMENT_URI		"DOCUMENT_URI"		/* excludes QS */
#define HTTP_REFERER		"HTTP_REFERER"
#define HTTP_USER_AGENT		"HTTP_USER_AGENT"
#define QUERY_STRING		"QUERY_STRING"
#define REMOTE_ADDR		"REMOTE_ADDR"
#define REMOTE_PORT		"REMOTE_PORT"
#define REQUEST_METHOD		"REQUEST_METHOD"
#define REQUEST_URI		"REQUEST_URI"		/* includes QS */
#define SERVER_NAME		"SERVER_NAME"
#define SERVER_PORT		"SERVER_PORT"
#define SERVER_PROTOCOL		"SERVER_PROTOCOL"	/* e.g., HTTP/1.1 */

/*
 * convert header @name to type @type
 */
struct convert_header_info {
	const char *name;
	data_type_t type;
};

enum {
	PAGE_ARCHIVE,
	PAGE_CATEGORY,
	PAGE_TAG,
	PAGE_COMMENT,
	PAGE_INDEX,
	PAGE_STORY,
	PAGE_XMLRPC,
	PAGE_ADMIN,
	PAGE_STATIC,
};

enum req_via {
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
	void *body;
	size_t bodylen;

	/* state */
	bool dump_latency;	/* request latency calculation */

	/* stats */
	struct {
		uint64_t fd_conn;	/* select(2) returned */
		uint64_t req_init;	/* request init */
		uint64_t enqueue;	/* enqueue */
		uint64_t dequeue;	/* dequeue */
		uint64_t req_output;	/* request output start */
		uint64_t req_done;	/* request output done */
		uint64_t destroy;	/* request destroy start */
	} stats;

	struct vars vars;

	char *fmt;		/* format (e.g., "html") */

	struct {
		int index_stories;
	} opts;

	FILE *out;
};

extern void req_init_scgi(struct req *req, int fd);
extern void req_destroy(struct req *req);
extern void req_output(struct req *req);
extern void req_head(struct req *req, const char *name, const char *val);
extern int req_dispatch(struct req *req);

extern void convert_headers(nvlist_t *, const struct convert_header_info *);

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
