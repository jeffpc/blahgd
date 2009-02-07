#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"

int main(int argc, char **argv)
{
	struct timespec s,e;
	char *path_info;
	int archid;
	struct post post;
	char nicetitle[32];

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

	snprintf(nicetitle, 32, "%d &raquo; %s", archid/100,
		 up_month_strs[(archid%100)-1]);

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
