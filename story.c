#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "post.h"
#include "sar.h"
#include "html.h"

int main(int argc, char **argv)
{
	char *path_info;
	struct post post;
	int ret;

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

	return 0;
}
