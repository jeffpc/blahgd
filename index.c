#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"
#include "main.h"

int blahg_index(int paged)
{
	struct timespec s,e;
	struct post post;

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post));
	post.out = stdout;
	post.title = "Blahg";

	fprintf(post.out, "Content-Type: text/html\n\n");

	post.page = max(paged, 0);

	html_header(&post);
	html_index(&post);
	html_sidebar(&post);
	html_footer(&post);

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
