#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sar.h"
#include "html.h"

/************************************************************************/
/*                                POST                                  */
/************************************************************************/
static void __invoke_for_each_post_cat(struct post *post, void(*f)(struct post*, char*))
{
	char *obuf;
	char tmp;
	int iidx, oidx;
	int done;

	if (!post->cats)
		return;

	obuf = malloc(strlen(post->cats));
	if (!obuf) {
		fprintf(post->out, "ERROR: could not alloc memory\n");
		return;
	}

	iidx = 0;
	oidx = 0;
	done = 0;
	while(!done) {
		tmp = post->cats[iidx];

		switch(tmp) {
			case '\0':
				done = 1;
				/* fall-thourgh */
			case ',':
				COPYCHAR(obuf, oidx, '\0');
				f(post, obuf);
				oidx=0;
				break;
			default:
				COPYCHAR(obuf, oidx, tmp);
				break;
		}

		iidx++;
	}

	free(obuf);
}

static void __story_cat_item(struct post *post, char *catname)
{
	cat(post, catname, "templates/story-cat-item.html", repltab_cat_html);
}

void html_story(struct post *post)
{
	cat(post, NULL, "templates/story-top.html", repltab_html);

	__invoke_for_each_post_cat(post, __story_cat_item);

	cat(post, NULL, "templates/story-middle.html", repltab_html);

	cat_post(post);

	cat(post, NULL, "templates/story-bottom.html", NULL);
}

/************************************************************************/
/*                           POST COMMENTS                              */
/************************************************************************/
static void __html_comment(struct post *post, struct comment *comm)
{
	cat(post, comm, "templates/story-comment-item-head.html",
	    repltab_comm_html);
	cat_post_comment(post, comm);
	cat(post, comm, "templates/story-comment-item-tail.html",
	    repltab_comm_html);
}

void html_comments(struct post *post)
{
	cat(post, NULL, "templates/story-comment-head.html", repltab_html);

	invoke_for_each_comment(post, __html_comment);

	cat(post, NULL, "templates/story-comment-tail.html", repltab_html);
}

/************************************************************************/
/*                                 SIDEBAR                              */
/************************************************************************/
static void __invoke_for_each_cat(struct post *post, char *prefix, void(*f)(struct post*, char*))
{
	struct stat statbuf;
	char path[FILENAME_MAX];
	struct dirent *de;
	DIR *dir;
	int ret;

	snprintf(path, FILENAME_MAX, "data/by-category/%s", prefix);
	dir = opendir(path);
	if (!dir)
		return;

	while((de = readdir(dir))) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		snprintf(path, FILENAME_MAX, "data/by-category/%s/%s", prefix, de->d_name);

		ret = lstat(path, &statbuf);
		if (ret == -1) {
			fprintf(post->out, "stat failed\n");
			break;
		}

		if (!S_ISDIR(statbuf.st_mode))
			continue;

		f(post, path+2+17);
		__invoke_for_each_cat(post, path+17, f);
	}

	closedir(dir);
}

static void __sidebar_cat_item(struct post *post, char *catname)
{
	cat(post, catname, "templates/sidebar-cat-item.html", repltab_cat_html);
}

void html_sidebar(struct post *post)
{
	cat(post, NULL, "templates/sidebar-top.html", repltab_html);

	__invoke_for_each_cat(post, ".", __sidebar_cat_item);

	cat(post, NULL, "templates/sidebar-middle.html", repltab_html);
#if 0
	for_each_month()
		cat(post, NULL, "templates/sidebar-archive-item.html", repltab_html);
#endif
	cat(post, NULL, "templates/sidebar-bottom.html", repltab_html);
}

/************************************************************************/
/*                             PAGE HEADER                              */
/************************************************************************/
void html_header(struct post *post)
{
	cat(post, NULL, "templates/header.html", repltab_html);
}

/************************************************************************/
/*                             PAGE FOOTER                              */
/************************************************************************/
void html_footer(struct post *post)
{
	cat(post, NULL, "templates/footer.html", repltab_html);
}
