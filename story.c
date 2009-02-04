#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "post.h"
#include "sar.h"
#include "html.h"

int main(int argc, char **argv)
{
	struct post post;
	int ret;

	post.out = stdout;

	fprintf(post.out, "Content-Type: text/html\n\n");

	ret = load_post(30, &post);
	if (ret) {
		fprintf(post.out, "Gah! %d\n", ret);
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
