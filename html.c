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
