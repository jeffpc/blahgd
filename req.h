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
#include <jeffpc/scgi.h>

#include "vars.h"

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

	const char *cat;
	const char *tag;
};

struct req {
	struct scgi *scgi;

	/* request */
	struct qs args;

	/* state */
	struct vars vars;

	struct str *fmt;	/* format (e.g., "html") */

	struct {
		int index_stories;
	} opts;
};

extern void req_init(struct req *req, struct scgi *scgi);
extern void req_destroy(struct req *req);
extern void req_output(struct req *req);
extern void req_head(struct req *req, const char *name, const char *val);
extern int req_dispatch(struct req *req);

extern int R404(struct req *req, char *tmpl);
extern int R301(struct req *req, const char *url);

extern int blahg_archive(struct req *req, int paged);
extern int blahg_category(struct req *req, const char *cat, int page);
extern int blahg_tag(struct req *req, const char *tag, int paged);
extern int blahg_comment(struct req *req);
extern int blahg_index(struct req *req, int paged);
extern int blahg_story(struct req *req);
extern int blahg_admin(struct req *req);

extern int init_wordpress_categories(void);

#endif
