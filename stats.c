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
