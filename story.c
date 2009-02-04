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
	struct post post;
	int ret;

	clock_gettime(CLOCK_REALTIME, &s);

	post.out = stdout;

	fprintf(post.out, "Content-Type: text/html\n\n");

	path_info = getenv("PATH_INFO");

	if (!path_info) {
		fprintf(post.out, "Invalid post #\n");
		return 0;
	}

	ret = load_post(atoi(path_info+1), &post);
	if (ret) {
		fprintf(post.out, "Gah! %d (postid=%d)\n", ret,
			atoi(path_info+1));
		return 0;
	}

	html_header(&post);
	html_story(&post);
	html_comments(&post);
	html_sidebar(&post);
	html_footer(&post);

	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
