#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"

static int validate_arch_id(int arch)
{
	int y = arch / 100;
	int m = arch % 100;

	return (m >= 1) && (m <= 12) && (y > 2000) && (y < 3000);
}

int blahg_archive(int m, int paged)
{
	struct timespec s,e;
	struct post_old post;
	char nicetitle[32];
	char pagetype[32];

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post_old));
	post.out = stdout;
	post.title = nicetitle;
	post.pagetype = pagetype;

	fprintf(post.out, "Content-Type: text/html\n\n");

	if (m == -1) {
		fprintf(post.out, "Invalid archival #\n");
		return 0;
	}

	snprintf(pagetype, 32, "%d", m);

	if (!validate_arch_id(m)) {
		m = 200001;
		post.pagetype = "200001";
	}

	snprintf(nicetitle, 32, "%d &raquo; %s", m/100,
		 up_month_strs[(m%100)-1]);

	post.page = max(paged, 0);

	feed_header(&post, "html");
	html_archive(&post, m);
	html_sidebar(&post);
	feed_footer(&post, "html");

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
