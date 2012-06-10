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
static void __invoke_for_each_post_cat(struct post *post, void(*f)(struct post*, char*, char*),
				       char *fmt)
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

static void __story_cat_item(struct post *post, char *catname, char *fmt)
{
	cat(post, catname, "story-cat-item", fmt, repltab_cat_html);
}

void html_story(struct post *post)
{
	cat(post, NULL, "story-top", "html", repltab_story_html);

	__invoke_for_each_post_cat(post, __story_cat_item, "html");

	cat(post, NULL, "story-middle", "html", repltab_story_html);

	cat_post(post);

	cat(post, NULL, "story-bottom-short", "html", NULL);
}

/************************************************************************/
/*                             INDEX                                    */
/************************************************************************/
void feed_index(struct post *post, char *fmt, int limit)
{
	sqlite3_stmt *stmt;
	int ret;

	open_db();
	SQL(stmt, "SELECT id FROM posts ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_INT(stmt, 1, limit);
	SQL_BIND_INT(stmt, 2, post->page * limit);
	SQL_FOR_EACH(stmt) {
		struct post p;
		int postid;

		postid = SQL_COL_INT(stmt, 0);
		p.out = post->out;

		if (load_post(postid, &p, 0))
			continue;

		cat(&p, NULL, "story-top", fmt, repltab_story_html);
		__invoke_for_each_post_cat(&p, __story_cat_item, fmt);
		if (!strcmp("atom", fmt)) {
			cat(&p, NULL, "story-middle-desc", "atom", repltab_story_html);
			//cat_post_preview(&p);
			cat_post(&p);
			cat(&p, NULL, "story-bottom-desc", "atom", repltab_story_html);
		}
		cat(&p, NULL, "story-middle", fmt, repltab_story_html);
		cat_post(&p);
		cat(&p, NULL, "story-bottom", fmt, repltab_story_numcomment_html);

		/* copy over the last time? */
		if (tm_cmp(&post->lasttime, &p.time) < 0)
			memcpy(&post->lasttime, &p.time, sizeof(struct tm));

		destroy_post(&p);
	}
}

/************************************************************************/
/*                           ARCHIVE INDEX                              */
/************************************************************************/
void html_archive(struct post *post, int archid)
{
	char fromtime[32];
	char totime[32];
	sqlite3_stmt *stmt;
	int toyear, tomonth;
	int ret;

	toyear = archid / 100;
	tomonth = (archid % 100) + 1;
	if (tomonth > 12) {
		tomonth = 1;
		toyear++;
	}

	snprintf(fromtime, sizeof(fromtime), "%04d-%02d-01 00:00",
		 archid / 100, archid % 100);
	snprintf(totime, sizeof(totime), "%04d-%02d-01 00:00",
		 toyear, tomonth);

	open_db();
	SQL(stmt, "SELECT id FROM posts WHERE time>=? AND time<? ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_STR(stmt, 1, fromtime);
	SQL_BIND_STR(stmt, 2, totime);
	SQL_BIND_INT(stmt, 3, HTML_ARCHIVE_STORIES);
	SQL_BIND_INT(stmt, 4, post->page * HTML_ARCHIVE_STORIES);
	SQL_FOR_EACH(stmt) {
		struct post p;
		int postid;

		postid = SQL_COL_INT(stmt, 0);
		p.out = post->out;

		if (load_post(postid, &p, 0))
			continue;

		cat(&p, NULL, "story-top", "html", repltab_story_html);
		__invoke_for_each_post_cat(&p, __story_cat_item, "html");
		cat(&p, NULL, "story-middle", "html", repltab_story_html);
		cat_post(&p);
		cat(&p, NULL, "story-bottom", "html", repltab_story_numcomment_html);

		destroy_post(&p);
	}

	cat(post, NULL, "archive-pager", "html", repltab_story_html);
}

/************************************************************************/
/*                         CATEGORY/TAG INDEX                           */
/************************************************************************/
void html_tag(struct post *post, char *tagname, char *bydir, int numstories)
{
	char path[FILENAME_MAX];
	char tag[FILENAME_MAX];
	sqlite3_stmt *stmt;
	int ret;

	snprintf(tag, sizeof(tag), "%%%s%%", tagname);

	open_db();
	if (!strcmp(bydir, "tag"))
		SQL(stmt, "SELECT id FROM posts WHERE tags LIKE ? ORDER BY time DESC LIMIT ? OFFSET ?");
	else
		SQL(stmt, "SELECT id FROM posts WHERE cats LIKE ? ORDER BY time DESC LIMIT ? OFFSET ?");

	SQL_BIND_STR(stmt, 1, tag);
	SQL_BIND_INT(stmt, 2, numstories);
	SQL_BIND_INT(stmt, 3, post->page * numstories);
	SQL_FOR_EACH(stmt) {
		struct post p;
		int postid;

		postid = SQL_COL_INT(stmt, 0);
		p.out = post->out;

		if (load_post(postid, &p, 0))
			continue;

		cat(&p, NULL, "story-top", "html", repltab_story_html);
		__invoke_for_each_post_cat(&p, __story_cat_item, "html");
		cat(&p, NULL, "story-middle", "html", repltab_story_html);
		cat_post(&p);
		cat(&p, NULL, "story-bottom", "html", repltab_story_numcomment_html);

		destroy_post(&p);
	}

	snprintf(path, FILENAME_MAX, "%s-pager", bydir);
	cat(post, NULL, path, "html", repltab_story_html);
}

/************************************************************************/
/*                           POST COMMENTS                              */
/************************************************************************/
static void __html_comment(struct post *post, struct comment *comm)
{
	cat(post, comm, "story-comment-item-head", "html",
	    repltab_comm_html);
	cat_post_comment(post, comm);
	cat(post, comm, "story-comment-item-tail", "html",
	    repltab_comm_html);
}

void html_comments(struct post *post)
{
	cat(post, NULL, "story-comment-head", "html", repltab_story_html);

	invoke_for_each_comment(post, __html_comment);

	cat(post, NULL, "story-comment-tail", "html", repltab_story_html);
}

/************************************************************************/
/*                                 SIDEBAR                              */
/************************************************************************/
static void __invoke_for_each_archive(struct post *post, void(*f)(struct post*, char*))
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

static void __sidebar_arch_item(struct post *post, char *archname)
{
	cat(post, archname, "sidebar-archive-item", "html", repltab_arch_html);
}

static void __tag_cloud(struct post *post)
{
	sqlite3_stmt *stmt;
	int ret;

	SQL(stmt, "SELECT tag, count(1) FROM post_tags GROUP BY tag ORDER BY tag");
	SQL_FOR_EACH(stmt) {
		const char *tag;

		tag = SQL_COL_STR(stmt, 0);

		cat(post, (char*) tag, "sidebar-tag-item", "html", repltab_cat_html);
	}
}

void html_sidebar(struct post *post)
{
	open_db();

	cat(post, NULL, "sidebar-top", "html", repltab_story_html);

	__tag_cloud(post);

	cat(post, NULL, "sidebar-middle", "html", repltab_story_html);

	__invoke_for_each_archive(post, __sidebar_arch_item);

	cat(post, NULL, "sidebar-bottom", "html", repltab_story_html);
}

void html_save_comment(struct post *post, int notsaved)
{
	cat(post, NULL, "story-top", "html", repltab_story_html);

	__invoke_for_each_post_cat(post, __story_cat_item, "html");

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
void feed_header(struct post *post, char *fmt)
{
	cat(post, NULL, "header", fmt, repltab_story_html);
}

/************************************************************************/
/*                             PAGE FOOTER                              */
/************************************************************************/
void feed_footer(struct post *post, char *fmt)
{
	cat(post, NULL, "footer", fmt, repltab_story_html);
}
