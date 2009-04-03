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

char *wordpress_catn[] = {
	[1]  = "miscellaneous",
	[2]  = "programming/kernel",
	[3]  = "school",
	[4]  = "work",
	[5]  = "random",
	[6]  = "programming",
	[7]  = "events/ols-2005",
	[8]  = "events",
	[9]  = "rants",
	[10] = "movies",
	[11] = "humor",
	[13] = "star-trek",
	[15] = "star-trek/tng",
	[16] = "star-trek/tos",
	[17] = "legal",
	[18] = "star-trek/voy",
	[19] = "star-trek/ent",
	[20] = "events/ols-2006",
	[21] = "fsl",
	[22] = "fsl/unionfs",
	[23] = "stargate/sg-1",
	[24] = "open-source",
	[25] = "astronomy",
	[26] = "programming/vcs",
	[27] = "programming/vcs/git",
	[28] = "programming/vcs/mercurial",
	[29] = "events/ols-2007",
	[30] = "programming/vcs/guilt",
	[31] = "photography",
	[34] = "music",
	[35] = "programming/mainframes",
	[36] = "events/sc-07",
	[39] = "hvf",
	[40] = "events/ols-2008",
	[41] = "sysadmin",
	[42] = "documentation",
	[43] = "stargate",
};

#define MIN_CATN 1
#define MAX_CATN 43

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
	int catn;

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

	catn = atoi(path_info+1);
	if (catn == 0) {
		/* string cat name */
		post.title = path_info+1;
	} else {
		/* wordpress cat name */
		if ((catn < MIN_CATN) || (catn > MAX_CATN)) {
			fprintf(post.out, "Go away!\n");
			return 0;
		}

		post.title = wordpress_catn[catn];

		if (!post.title) {
			fprintf(post.out, "Go away.\n");
			return 0;
		}
	}

	html_header(&post);
	html_category(&post, post.title);
	html_sidebar(&post);
	html_footer(&post);

	post.title = NULL;
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
