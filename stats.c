#include <strings.h>

#include "stats.h"
#include "atomic.h"

/* enough for ~110 seconds worst case */
#define NUM_BUCKETS	40

struct bucket {
	atomic_t render;
	atomic_t send;
};

static struct bucket data[NUM_STATPAGE][NUM_BUCKETS];

static inline int bucket(uint64_t lat)
{
	int idx;

	idx = flsll(lat);

	return (idx >= NUM_BUCKETS) ? (NUM_BUCKETS - 1) : idx;
}

#define INC(b, x, y)	atomic_inc(&b[bucket(x)].y)

void stats_update_request(enum statpage page, struct stats *s)
{
	struct bucket *b = data[page];

	INC(b, s->req_output - s->req_init, render);
	INC(b, s->req_done - s->req_output, send);
}
