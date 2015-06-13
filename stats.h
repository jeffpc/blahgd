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
	STATPAGE_XMLRPC,
	STATPAGE_FEED_RSS2,
	STATPAGE_FEED_ATOM, /* don't forget to update NUM_STATPAGE */
};

#define NUM_STATPAGE	(STATPAGE_FEED_ATOM + 1)

extern void stats_update_request(enum statpage, struct stats *);

#endif
