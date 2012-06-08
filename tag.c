#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "post.h"
#include "sar.h"
#include "html.h"
#include "utils.h"

int blahg_tag(char *tag, int paged)
{
	struct timespec s,e;
	struct post post;

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post));
	post.out = stdout;

	fprintf(post.out, "Content-Type: text/html\n\n");

	if (!tag) {
		fprintf(post.out, "Invalid tag name\n");
		return 0;
	}

	/*
	 * SECURITY CHECK: make sure no one is trying to give us a '..' in
	 * the path
	 */
	if (hasdotdot(tag)) {
		fprintf(post.out, "Go away\n");
		return 0;
	}

	/* string cat name */
	post.title = tag;
	post.pagetype = post.title;

	post.page = max(paged,0);

	feed_header(&post, "html");
	html_tag(&post, post.title, "tag", HTML_TAG_STORIES);
	html_sidebar(&post);
	feed_footer(&post, "html");

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
