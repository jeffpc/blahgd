#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "post.h"
#include "sar.h"
#include "html.h"
#include "config_opts.h"

int blahg_feed(char *feed, int p)
{
	struct timespec s,e;
	struct post_old post;

	if (strcmp(feed, "atom"))
		disp_404("Atom only",
			 "When I first decided to write my own blogging "
			 "system, I made a decision that the only feed type "
			 "I'd support (at least for now) would be the Atom "
			 "format.  Yes, there are a lot of RSS and RSS2 feeds "
			 "out there, but it seems to be the case that Atom is "
			 "just as supported (and a bit easier to generate). "
			 "Feel free to contact me if you cannot live without "
			 "a non-Atom feed.");

	if (p != -1)
		disp_404("Comment feed not yet supported",
			 "It turns out that there are some features that I "
			 "never got around to implementing.  You just found "
			 "one of them.  Eventually, ");

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post_old));
	post.out = stdout;
	post.title = "Blahg";

	fprintf(post.out, "Content-Type: application/atom+xml; charset=UTF-8\n\n");

	feed_header(&post,"atom");
	feed_index(&post, "atom", FEED_INDEX_STORIES);
	feed_footer(&post,"atom");

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
