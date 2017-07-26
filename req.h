/*
 * Copyright (c) 2014-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __REQ_H
#define __REQ_H

#include <stdbool.h>

#include <jeffpc/scgisvc.h>

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

enum {
	PAGE_ARCHIVE,
	PAGE_CATEGORY,
	PAGE_TAG,
	PAGE_COMMENT,
	PAGE_INDEX,
	PAGE_STORY,
	PAGE_ADMIN,
	PAGE_STATIC,
};

struct qs {
	int page;

	int p;
	int paged;
	int m;
	int preview;
	int admin;
	int comment;
	const char *cat;
	const char *tag;
	const char *feed;
};

struct req {
	struct scgi *scgi;

	/* request */
	struct nvlist *request_qs;
	struct qs args;

	/* response */
	int write_errno;	/* xwrite() return in req_output() */

	/* state */
	struct vars vars;

	const char *fmt;	/* format (e.g., "html") */

	struct {
		int index_stories;
	} opts;

	int fd;
};

extern void init_req_subsys(void);
extern struct req *req_alloc(void);
extern void req_free(struct req *req);

extern void req_init(struct req *req);
extern void req_destroy(struct req *req);
extern void req_output(struct req *req);
extern void req_head(struct req *req, const char *name, const char *val);
extern int req_dispatch(struct req *req);

extern int R404(struct req *req, char *tmpl);
extern int R301(struct req *req, const char *url);

extern int blahg_archive(struct req *req, int m, int paged);
extern int blahg_category(struct req *req, const char *cat, int page);
extern int blahg_tag(struct req *req, const char *tag, int paged);
extern int blahg_comment(struct req *req);
extern int blahg_index(struct req *req, int paged);
extern int blahg_story(struct req *req, int p, bool preview);
extern int blahg_admin(struct req *req);

extern int init_wordpress_categories(void);

#endif
