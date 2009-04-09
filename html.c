#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "sar.h"
#include "html.h"
#include "dir.h"

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

static void __story_cat_item_feed(struct post *post, char *catname)
{
	cat(post, catname, "templates/story-cat-item.atom", repltab_cat_html);
}

void html_story(struct post *post)
{
	cat(post, NULL, "templates/story-top.html", repltab_story_html);

	__invoke_for_each_post_cat(post, __story_cat_item);

	cat(post, NULL, "templates/story-middle.html", repltab_story_html);

	cat_post(post);

	cat(post, NULL, "templates/story-bottom-short.html", NULL);
}

/************************************************************************/
/*                             INDEX                                    */
/************************************************************************/
static void __each_index_helper(struct post *post, char *name, void *data)
{
	int postid = atoi(name);
	struct post p;

	memset(&p, 0, sizeof(struct post));
	p.out = post->out;

	if (load_post(postid, &p))
		return;

	cat(&p, NULL, "templates/story-top.html", repltab_story_html);
	__invoke_for_each_post_cat(&p, __story_cat_item);
	cat(&p, NULL, "templates/story-middle.html", repltab_story_html);
	cat_post(&p);
	cat(&p, NULL, "templates/story-bottom.html", repltab_story_numcomment_html);

	destroy_post(&p);
}

void html_index(struct post *post)
{
	DIR *dir;

	dir = opendir("data/posts");
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __each_index_helper, NULL, SORT_DESC,
			    post->page*HTML_INDEX_STORIES, HTML_INDEX_STORIES);

	closedir(dir);
}

static void __each_feed_index_helper(struct post *post, char *name, void *data)
{
	int postid = atoi(name);
	struct post p;

	memset(&p, 0, sizeof(struct post));
	p.out = post->out;

	if (load_post(postid, &p))
		return;

	cat(&p, NULL, "templates/story-top.atom", repltab_story_numcomment_html);
	__invoke_for_each_post_cat(&p, __story_cat_item_feed);
	cat(&p, NULL, "templates/story-middle-desc.atom", repltab_story_html);
	//cat_post_preview(&p);
	cat_post(&p);
	cat(&p, NULL, "templates/story-bottom-desc.atom", repltab_story_html);
	cat(&p, NULL, "templates/story-middle.atom", repltab_story_html);
	cat_post(&p);
	cat(&p, NULL, "templates/story-bottom.atom", NULL);

	destroy_post(&p);
}

void feed_index(struct post *post, char *fmt)
{
	DIR *dir;

	dir = opendir("data/posts");
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __each_feed_index_helper, NULL, SORT_DESC,
			    0, FEED_INDEX_STORIES);

	closedir(dir);
}

/************************************************************************/
/*                           ARCHIVE INDEX                              */
/************************************************************************/
void html_archive(struct post *post, int archid)
{
	char path[FILENAME_MAX];
	DIR *dir;
	int page=0; //FIXME

	snprintf(path, FILENAME_MAX, "data/by-month/%d", archid);

	dir = opendir(path);
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __each_index_helper, NULL, SORT_DESC,
			    HTML_ARCHIVE_STORIES*page, HTML_ARCHIVE_STORIES);

	closedir(dir);
}

/************************************************************************/
/*                          CATEGORY INDEX                              */
/************************************************************************/
void html_category(struct post *post, char *catname)
{
	char path[FILENAME_MAX];
	DIR *dir;
	int page=0; //FIXME

	snprintf(path, FILENAME_MAX, "data/by-category/%s/", catname);

	dir = opendir(path);
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __each_index_helper, NULL, SORT_DESC,
			    HTML_CATEGORY_STORIES*page, HTML_CATEGORY_STORIES);

	closedir(dir);
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
	cat(post, NULL, "templates/story-comment-head.html", repltab_story_html);

	invoke_for_each_comment(post, __html_comment);

	cat(post, NULL, "templates/story-comment-tail.html", repltab_story_html);
}

/************************************************************************/
/*                                 SIDEBAR                              */
/************************************************************************/
static void __invoke_for_each_cat(struct post *post, char *prefix,
				  void(*f)(struct post*, char*));

static void __each_cat_helper(struct post *post, char *name, void *data)
{
	char *prefix = ((char**)data)[0];
	void(*f)(struct post*, char*) = ((void**)data)[1];

	char path[FILENAME_MAX];
	struct stat statbuf;
	int ret;

	snprintf(path, FILENAME_MAX, "data/by-category/%s/%s", prefix, name);

	ret = lstat(path, &statbuf);
	if (ret == -1) {
		fprintf(post->out, "stat failed\n");
		return;
	}

	if (!S_ISDIR(statbuf.st_mode))
		return;

	f(post, path+2+17);
	__invoke_for_each_cat(post, path+17, f);
}

static void __invoke_for_each_cat(struct post *post, char *prefix,
				  void(*f)(struct post*, char*))
{
	char path[FILENAME_MAX];
	DIR *dir;
	void *plist[2] = {prefix, f};

	snprintf(path, FILENAME_MAX, "data/by-category/%s", prefix);
	dir = opendir(path);
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __each_cat_helper, plist,
			    SORT_ASC | SORT_STRING, 0, -1);

	closedir(dir);
}

static void __cb_wrap(struct post *post, char *name, void *data)
{
	void(*f)(struct post*, char*) = data;

	f(post, name);
}

static void __invoke_for_each_archive(struct post *post, void(*f)(struct post*, char*))
{
	DIR *dir;

	dir = opendir("data/by-month");
	if (!dir)
		return;

	sorted_readdir_loop(dir, post, __cb_wrap, f, SORT_DESC, 0, -1);

	closedir(dir);
}

static void __sidebar_cat_item(struct post *post, char *catname)
{
	cat(post, catname, "templates/sidebar-cat-item.html", repltab_cat_html);
}

static void __sidebar_arch_item(struct post *post, char *archname)
{
	cat(post, archname, "templates/sidebar-archive-item.html", repltab_arch_html);
}

void html_sidebar(struct post *post)
{
	cat(post, NULL, "templates/sidebar-top.html", repltab_story_html);

	__invoke_for_each_cat(post, ".", __sidebar_cat_item);

	cat(post, NULL, "templates/sidebar-middle.html", repltab_story_html);

	__invoke_for_each_archive(post, __sidebar_arch_item);

	cat(post, NULL, "templates/sidebar-bottom.html", repltab_story_html);
}

/************************************************************************/
/*                             PAGE HEADER                              */
/************************************************************************/
void html_header(struct post *post)
{
	cat(post, NULL, "templates/header.html", repltab_story_html);
}

void feed_header(struct post *post, char *fmt)
{
	cat(post, NULL, "templates/header.atom", repltab_story_html);
}

/************************************************************************/
/*                             PAGE FOOTER                              */
/************************************************************************/
void html_footer(struct post *post)
{
	cat(post, NULL, "templates/footer.html", repltab_story_html);
}

void feed_footer(struct post *post, char *fmt)
{
	cat(post, NULL, "templates/footer.atom", repltab_story_html);
}
