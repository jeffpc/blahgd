#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "post.h"
#include "sar.h"
#include "html.h"

#define HDD_START	0
#define HDD_1DOT	1
#define HDD_2DOT	2
#define HDD_OK		3

int hasdotdot(char *path)
{
	int state = HDD_START;
	char tmp;

	while(*path) {
		tmp = *path;

		switch(tmp) {
			case '.':
				if (state == HDD_START)
					state = HDD_1DOT;
				else if (state == HDD_1DOT)
					state = HDD_2DOT;
				else
					state = HDD_OK;
				break;
			case '/':
				if (state == HDD_2DOT)
					return 1; /* '/../' found */

				state = HDD_START;
				break;
			default:
				state = HDD_OK;
				break;
		}

		path++;
	}

	return (state == HDD_2DOT);
}

int main(int argc, char **argv)
{
	struct timespec s,e;
	char *path_info;
	struct post post;

	clock_gettime(CLOCK_REALTIME, &s);

	memset(&post, 0, sizeof(struct post));
	post.out = stdout;

	fprintf(post.out, "Content-Type: text/html\n\n");

	path_info = getenv("PATH_INFO");

	if (!path_info) {
		fprintf(post.out, "Invalid category name\n");
		return 0;
	}

	/*
	 * SECURITY CHECK: make sure no one is trying to give us a '..' in
	 * the path
	 */
	if (hasdotdot(path_info)) {
		fprintf(post.out, "Go away\n");
		return 0;
	}

	post.title = path_info+1;

	html_header(&post);
	html_category(&post, path_info+1);
	html_sidebar(&post);
	html_footer(&post);

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
