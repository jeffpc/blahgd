#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"

int blahg_story(int p, int preview)
{
	struct timespec s,e;
	struct post post;
	int ret;

	clock_gettime(CLOCK_REALTIME, &s);

	post.out = stdout;

	fprintf(post.out, "Content-Type: text/html\n\n");

	if (p == -1) {
		fprintf(post.out, "Invalid post #\n");
		return 0;
	}

	ret = load_post(p, &post, preview);
	if (ret) {
		fprintf(post.out, "Gah! %d (postid=%d)\n", ret, p);
		return 0;
	}

	feed_header(&post, "html");
	html_story(&post);
	if (!preview)
		html_comments(&post);
	html_sidebar(&post);
	feed_footer(&post, "html");

	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
