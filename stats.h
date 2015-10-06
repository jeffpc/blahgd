/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#ifndef __STATS_H
#define __STATS_H

#include <inttypes.h>

struct stats {
	uint64_t fd_conn;	/* select(2) returned */
	uint64_t req_init;	/* request init */
	uint64_t enqueue;	/* enqueue */
	uint64_t dequeue;	/* dequeue */
	uint64_t req_output;	/* request output start */
	uint64_t req_done;	/* request output done */
	uint64_t destroy;	/* request destroy start */
};

enum statpage {
	STATPAGE_HTTP_301,
	STATPAGE_HTTP_404,
	STATPAGE_HTTP_XXX,

	/* the following imply HTTP status 200 */
	STATPAGE_STATIC,
	STATPAGE_INDEX,
	STATPAGE_STORY,
	STATPAGE_COMMENT,
	STATPAGE_ARCHIVE,
	STATPAGE_TAG,
	STATPAGE_CAT,
	STATPAGE_ADMIN,
	STATPAGE_FEED_RSS2,
	STATPAGE_FEED_ATOM, /* don't forget to update NUM_STATPAGE */
};

#define NUM_STATPAGE	(STATPAGE_FEED_ATOM + 1)

extern void stats_update_request(enum statpage, struct stats *);

#endif
