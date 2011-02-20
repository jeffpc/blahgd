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

int blahg_archive(int argc, char **argv)
{
	struct timespec s,e;
	char *path_info;
	int archid;
	struct post post;
	char nicetitle[32];
	char *pn;

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post));
	post.out = stdout;
	post.title = nicetitle;

	fprintf(post.out, "Content-Type: text/html\n\n");

	path_info = getenv("PATH_INFO");

	if (!path_info) {
		fprintf(post.out, "Invalid archival #\n");
		return 0;
	}

	archid = atoi(path_info+1);
	post.pagetype = path_info+1;

	if (!validate_arch_id(archid)) {
		archid = 200001;
		post.pagetype = "200001";
	}

	snprintf(nicetitle, 32, "%d &raquo; %s", archid/100,
		 up_month_strs[(archid%100)-1]);

	pn = getenv("QUERY_STRING");
	if (!pn)
		post.page = 0;
	else
		post.page = atoi(pn);

	html_header(&post);
	html_archive(&post, archid);
	html_sidebar(&post);
	html_footer(&post);

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
