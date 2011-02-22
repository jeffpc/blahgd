#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"

int blahg_feed(char *feed, int p)
{
	struct timespec s,e;
	struct post post;

	if (strcmp(feed, "atom")) {
		printf("Status: 404 Not Found\n\nNot found.\n");
		return 0;
	}

	if (p != -1) {
		printf("Status: 404 Not Found\n\nComment feed not yet supported.\n");
		return 0;
	}

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post));
	post.out = stdout;
	post.title = "Blahg";

	fprintf(post.out, "Content-Type: application/atom+xml; charset=UTF-8\n\n");

	feed_header(&post,"atom");
	feed_index(&post,"atom");
	feed_footer(&post,"atom");

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
