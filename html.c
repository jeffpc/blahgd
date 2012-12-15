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

#include "db.h"

/************************************************************************/
/*                                POST                                  */
/************************************************************************/
static void __invoke_for_each_post_tag(struct post_old *post, void(*f)(struct post_old*, char*, char*),
				       char *fmt)
{
	char *obuf;
	char tmp;
	int iidx, oidx;
	int done;

	if (!post->tags)
		return;

	obuf = malloc(strlen(post->tags)+1);
	if (!obuf) {
		fprintf(post->out, "ERROR: could not alloc memory\n");
		return;
	}

	iidx = 0;
	oidx = 0;
	done = 0;
	while(!done) {
		tmp = post->tags[iidx];

		switch(tmp) {
			case '\0':
				done = 1;
				/* fall-thourgh */
			case ',':
				COPYCHAR(obuf, oidx, '\0');
				f(post, obuf, fmt);
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

static void __story_tag_item(struct post_old *post, char *tagname, char *fmt)
{
	cat(post, tagname, "story-tag-item", fmt, repltab_tag_html);
}

void html_story(struct post_old *post)
{
	cat(post, NULL, "story-top", "html", repltab_story_html);

	__invoke_for_each_post_tag(post, __story_tag_item, "html");

	cat(post, NULL, "story-middle", "html", repltab_story_html);

	cat_post(post);

	cat(post, NULL, "story-bottom-short", "html", NULL);
}

/************************************************************************/
/*                             INDEX                                    */
/************************************************************************/
void feed_index(struct post_old *post, char *fmt, int limit)
{
}

/************************************************************************/
/*                           ARCHIVE INDEX                              */
/************************************************************************/
void html_archive(struct post_old *post, int archid)
{
}

/************************************************************************/
/*                         CATEGORY/TAG INDEX                           */
/************************************************************************/
void html_tag(struct post_old *post, char *tagname, char *bydir, int numstories)
{
}

/************************************************************************/
/*                           POST COMMENTS                              */
/************************************************************************/
static void __html_comment(struct post_old *post, struct comment *comm)
{
	cat(post, comm, "story-comment-item-head", "html",
	    repltab_comm_html);
	cat_post_comment(post, comm);
	cat(post, comm, "story-comment-item-tail", "html",
	    repltab_comm_html);
}

void html_comments(struct post_old *post)
{
	cat(post, NULL, "story-comment-head", "html", repltab_story_html);

	invoke_for_each_comment(post, __html_comment);

	cat(post, NULL, "story-comment-tail", "html", repltab_story_html);
}

/************************************************************************/
/*                                 SIDEBAR                              */
/************************************************************************/
static void __invoke_for_each_archive(struct post_old *post, void(*f)(struct post_old*, char*))
{
	sqlite3_stmt *stmt;
	int ret;

	open_db();
	SQL(stmt, "SELECT DISTINCT STRFTIME(\"%Y%m\", time) AS t FROM posts ORDER BY t DESC");
	SQL_FOR_EACH(stmt) {
		const char *archid;

		archid = SQL_COL_STR(stmt, 0);
		f(post, (char*) archid);
	}
}

static void __sidebar_arch_item(struct post_old *post, char *archname)
{
	cat(post, archname, "sidebar-archive-item", "html", repltab_arch_html);
}

static void __tag_cloud(struct post_old *post)
{
	sqlite3_stmt *stmt;
	int ret;

	SQL(stmt, "SELECT tag, count(1) FROM post_tags GROUP BY tag ORDER BY tag");
	SQL_FOR_EACH(stmt) {
		const char *tag;

		tag = SQL_COL_STR(stmt, 0);

		cat(post, (char*) tag, "sidebar-tag-item", "html", repltab_tag_html);
	}
}

void html_sidebar(struct post_old *post)
{
	open_db();

	cat(post, NULL, "sidebar-top", "html", repltab_story_html);

	__tag_cloud(post);

	cat(post, NULL, "sidebar-middle", "html", repltab_story_html);

	__invoke_for_each_archive(post, __sidebar_arch_item);

	cat(post, NULL, "sidebar-bottom", "html", repltab_story_html);
}

void html_save_comment(struct post_old *post, int notsaved)
{
	cat(post, NULL, "story-top", "html", repltab_story_html);

	__invoke_for_each_post_tag(post, __story_tag_item, "html");

	if (notsaved)
		cat(post, NULL, "story-comment-notsaved", "html", repltab_story_html);
	else
		cat(post, NULL, "story-comment-saved", "html", repltab_story_html);

	//cat_post(post);

	cat(post, NULL, "story-bottom-short", "html", NULL);
}

/************************************************************************/
/*                             PAGE HEADER                              */
/************************************************************************/
void feed_header(struct post_old *post, char *fmt)
{
	cat(post, NULL, "header", fmt, repltab_story_html);
}

/************************************************************************/
/*                             PAGE FOOTER                              */
/************************************************************************/
void feed_footer(struct post_old *post, char *fmt)
{
	cat(post, NULL, "footer", fmt, repltab_story_html);
}
