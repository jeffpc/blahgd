#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "main.h"
#include "post.h"
#include "html.h"

static char *nullterminate(char *s)
{
	while(*s && (*s != '&'))
		s++;

	*s = '\0';

	return s + 1;
}

static void parse_qs(char *qs, struct qs *args)
{
	char *tmp;
	char *end;

	args->page = PAGE_INDEX;
	args->p = -1;
	args->paged = -1;
	args->m = -1;
	args->xmlrpc = 0;
	args->cat = NULL;
	args->tag = NULL;
	args->feed = NULL;
	args->comment = NULL;
	args->preview = 0;

	if (!qs)
		return;

	end = qs + strlen(qs);
	tmp = qs;

	while (end > tmp) {
		int *iptr = NULL;
		char **cptr = NULL;
		int len;

		if (!strncmp(qs, "p=", 2)) {
			iptr = &args->p;
			len = 2;
		} else if (!strncmp(qs, "paged=", 6)) {
			iptr = &args->paged;
			len = 6;
		} else if (!strncmp(qs, "m=", 2)) {
			iptr = &args->m;
			len = 2;
		} else if (!strncmp(qs, "cat=", 4)) {
			cptr = &args->cat;
			len = 4;
		} else if (!strncmp(qs, "tag=", 4)) {
			cptr = &args->tag;
			len = 4;
		} else if (!strncmp(qs, "feed=", 5)) {
			cptr = &args->feed;
			len = 5;
		} else if (!strncmp(qs, "comment=", 8)) {
			cptr = &args->comment;
			len = 8;
		} else if (!strncmp(qs, "preview=", 8)) {
			iptr = &args->preview;
			len = 8;
		} else if (!strncmp(qs, "xmlrpc=", 7)) {
			iptr = &args->xmlrpc;
			len = 7;
		} else {
			args->page = PAGE_MALFORMED;
			return;
		}

		qs += len;
		tmp = nullterminate(qs);

		if (iptr)
			*iptr = atoi(qs);
		else if (cptr)
			*cptr = qs;

		qs = tmp;
	}

	if (args->xmlrpc)
		args->page = PAGE_XMLRPC;
	else if (args->comment)
		args->page = PAGE_COMMENT;
	else if (args->feed)
		args->page = PAGE_FEED;
	else if (args->tag)
		args->page = PAGE_TAG;
	else if (args->cat)
		args->page = PAGE_CATEGORY;
	else if (args->m != -1)
		args->page = PAGE_ARCHIVE;
	else if (args->p != -1)
		args->page = PAGE_STORY;
}

static int blahg_malformed(int argc, char **argv)
{
	printf("Content-Type: text/plain\n\n");
	printf("There has been an error processing your request.  The site "
	       "administrator has\nbeen notified.\n\nSorry for the "
	       "inconvenience.\n\nJosef 'Jeff' Sipek.\n");

	// FIXME: send $SCRIPT_URL, $PATH_INFO, and $QUERY_STRING via email

	return 0;
}

void disp_404(char *title, char *txt)
{
	struct post post;

	memset(&post, 0, sizeof(struct post));
	post.out = stdout;
	post.title = "Error";

	fprintf(post.out, "Status: 404 Not Found\nContent-Type: text/html\n\n");

	html_header(&post);
	printf("<h2>%s</h2>\n<div class=\"storycentent\">%s</div>\n", title, txt);

	html_sidebar(&post);
	html_footer(&post);

	post.title = NULL;
	destroy_post(&post);

	exit(0);
}

int main(int argc, char **argv)
{
	struct qs args;

	parse_qs(getenv("QUERY_STRING"), &args);

	printf("X-Pingback: http://blahg-test.josefsipek.net/?xmlrpc=1\n");

	switch(args.page) {
		case PAGE_ARCHIVE:
			return blahg_archive(args.m, args.paged);
		case PAGE_CATEGORY:
			return blahg_category(args.cat, args.paged);
		case PAGE_TAG:
			return blahg_tag(args.tag, args.paged);
		case PAGE_COMMENT:
			return blahg_comment();
		case PAGE_FEED:
			return blahg_feed(args.feed, args.p);
		case PAGE_INDEX:
			return blahg_index(args.paged);
		case PAGE_STORY:
			return blahg_story(args.p, args.preview);
		case PAGE_XMLRPC:
			return blahg_pingback();
		default:
			return blahg_malformed(argc, argv);
	}

	return 0;
}
